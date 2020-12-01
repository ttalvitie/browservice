#include "http.hpp"

#include "task_queue.hpp"

#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/SocketAddress.h>

namespace retrojsvice {

namespace {

// We use a AliveToken to track that all the relevant Poco HTTP server
// background threads actually shut down before reporting successful shutdown.
class AliveToken {
public:
    static AliveToken create() {
        return AliveToken();
    }

private:
    AliveToken() : inner_(make_shared<Inner>()) {}

    struct Inner {};
    shared_ptr<Inner> inner_;

    friend class AliveTokenWatcher;
};

class AliveTokenWatcher {
public:
    AliveTokenWatcher(AliveToken aliveToken) : inner_(aliveToken.inner_) {}

    bool isTokenAlive() {
        return !inner_.expired();
    }

private:
    weak_ptr<AliveToken::Inner> inner_;
};

class HTTPRequestHandler : public Poco::Net::HTTPRequestHandler {
public:
    HTTPRequestHandler(
        weak_ptr<HTTPServerEventHandler> eventHandler,
        shared_ptr<TaskQueue> taskQueue,
        AliveToken aliveToken
    )
        : eventHandler_(eventHandler),
          taskQueue_(taskQueue),
          aliveToken_(aliveToken)
    {}

    virtual void handleRequest(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response
    ) override {
        ActiveTaskQueueLock activeTaskQueueLock(taskQueue_);

        INFO_LOG("Got HTTP request");
    }

private:
    weak_ptr<HTTPServerEventHandler> eventHandler_;
    shared_ptr<TaskQueue> taskQueue_;
    AliveToken aliveToken_;
};

class HTTPRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
    HTTPRequestHandlerFactory(
        weak_ptr<HTTPServerEventHandler> eventHandler,
        shared_ptr<TaskQueue> taskQueue,
        AliveToken aliveToken
    )
        : eventHandler_(eventHandler),
          taskQueue_(taskQueue),
          aliveToken_(aliveToken)
    {}

    virtual Poco::Net::HTTPRequestHandler* createRequestHandler(
        const Poco::Net::HTTPServerRequest& request
    ) override {
        return new HTTPRequestHandler(eventHandler_, taskQueue_, aliveToken_);
    }

private:
    weak_ptr<HTTPServerEventHandler> eventHandler_;
    shared_ptr<TaskQueue> taskQueue_;
    AliveToken aliveToken_;
};

}

struct SocketAddress::Impl {
    Poco::Net::SocketAddress addr;
    string addrStr;
};

optional<SocketAddress> SocketAddress::parse(string repr) {
    SocketAddress ret;
    ret.impl_ = make_shared<Impl>();
    try {
        ret.impl_->addr = Poco::Net::SocketAddress(repr);
        ret.impl_->addrStr = ret.impl_->addr.toString();
    } catch(...) {
        return {};
    }
    return ret;
}

SocketAddress::SocketAddress() {}

ostream& operator<<(ostream& out, SocketAddress addr) {
    out << addr.impl_->addrStr;
    return out;
}

class HTTPServer::Impl : public enable_shared_from_this<Impl> {
SHARED_ONLY_CLASS(Impl);
public:
    // May throw Poco::Exception if startup fails.
    Impl(CKey,
        weak_ptr<HTTPServerEventHandler> eventHandler,
        SocketAddress listenAddr,
        int maxThreads
    )
        : eventHandler_(eventHandler),
          state_(Running),
          aliveToken_(AliveToken::create()),
          threadPool_(1, maxThreads),
          socketAddress_(listenAddr.impl_->addr),
          serverSocket_(socketAddress_)
    {
        httpServer_.emplace(
            new HTTPRequestHandlerFactory(
                eventHandler,
                TaskQueue::getActiveQueue(),
                aliveToken_
            ),
            threadPool_,
            serverSocket_,
            new Poco::Net::HTTPServerParams()
        );
        httpServer_->start();
    }
    ~Impl() {
        REQUIRE(state_ == ShutdownComplete);
    }

    void shutdown() {
        REQUIRE(state_ == Running);

        INFO_LOG("Shutting down HTTP server");
        state_ = ShutdownPending;

        shared_ptr<Impl> self = shared_from_this();
        shared_ptr<TaskQueue> taskQueue = TaskQueue::getActiveQueue();
        thread([self, taskQueue]() {
            ActiveTaskQueueLock activeTaskQueueLock(taskQueue);

            try {
                // Do not accept new connections
                self->httpServer_->stop();

                // 1s grace time for current connections before abort
                for(int i = 0; i < 10; ++i) {
                    if(self->httpServer_->currentConnections() == 0) {
                        break;
                    }
                    sleep_for(milliseconds(100));
                }
                self->httpServer_->stopAll(true);

                self->httpServer_.reset();
            } catch(const Poco::Exception& e) {
                PANIC(
                    "Shutting down Poco HTTP server failed with exception: ",
                    e.displayText()
                );
            }

            // Just to be safe, use our alive token to wait for possibly
            // lingering Poco HTTP server background threads to actually shut
            // down so that we can be sure that we won't get any calls to the
            // HTTP request handler after shutdown.
            AliveTokenWatcher watcher(move(self->aliveToken_));
            while(watcher.isTokenAlive()) {
                sleep_for(milliseconds(100));
            }

            postTask([self]() {
                REQUIRE_API_THREAD();
                REQUIRE(self->state_ == ShutdownPending);

                self->state_ = ShutdownComplete;
                INFO_LOG("HTTP server shutdown complete");

                postTask(
                    self->eventHandler_,
                    &HTTPServerEventHandler::onHTTPServerShutdownComplete
                );
            });
        }).detach();
    }

    bool isShutdownComplete() {
        return state_ == ShutdownComplete;
    }

private:
    weak_ptr<HTTPServerEventHandler> eventHandler_;

    enum {Running, ShutdownPending, ShutdownComplete} state_;

    AliveToken aliveToken_;

    Poco::ThreadPool threadPool_;
    Poco::Net::SocketAddress socketAddress_;
    Poco::Net::ServerSocket serverSocket_;
    optional<Poco::Net::HTTPServer> httpServer_;
};

HTTPServer::HTTPServer(CKey,
    weak_ptr<HTTPServerEventHandler> eventHandler,
    SocketAddress listenAddr,
    int maxThreads
) {
    REQUIRE_API_THREAD();
    REQUIRE(maxThreads > 0);

    INFO_LOG("Starting HTTP server (listen address: ", listenAddr, ")");

    try {
        impl_ = Impl::create(eventHandler, listenAddr, maxThreads);
    } catch(const Poco::Exception& e) {
        PANIC("Starting Poco HTTP server failed with exception: ", e.displayText());
    }

    INFO_LOG("HTTP server started successfully");
}

HTTPServer::~HTTPServer() {
    REQUIRE(impl_->isShutdownComplete());
}

void HTTPServer::shutdown() {
    REQUIRE_API_THREAD();
    impl_->shutdown();
}

}
