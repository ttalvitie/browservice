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

}

class HTTPRequest::Impl {
public:
    // May throw Poco::Exception.
    Impl(
        Poco::Net::HTTPServerRequest& request,
        promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise,
        AliveToken aliveToken
    )
        : aliveToken_(aliveToken),
          request_(&request),
          method_(request.getMethod()),
          path_(request.getURI()),
          userAgent_(request.get("User-Agent", "")),
          formParsed_(false),
          responderPromise_(move(responderPromise))
    {
        REQUIRE(request_ != nullptr);
    }

    ~Impl() {
        if(request_ != nullptr) {
            WARNING_LOG("HTTP response not provided, sending internal server error");
            sendTextResponse(
                500,
                "ERROR: Request handling failure\n",
                true,
                {}
            );
        }
    }

    DISABLE_COPY_MOVE(Impl);

    string method() {
        REQUIRE(request_ != nullptr);
        return method_;
    }
    string path() {
        REQUIRE(request_ != nullptr);
        return path_;
    }
    string userAgent() {
        REQUIRE(request_ != nullptr);
        return userAgent_;
    }

    string getFormParam(string name) {
        REQUIRE(request_ != nullptr);

        if(!formParsed_) {
            formParsed_ = true;
            try {
                if(method() == "POST") {
                    form_.emplace(*request_, request_->stream());
                } else {
                    form_.emplace();
                }
            } catch(const Poco::Exception& e) {
                WARNING_LOG(
                    "Parsing HTML form with Poco failed with exception ",
                    "(defaulting to empty): ", e.displayText()
                );
            }
        }

        if(form_) {
            try {
                return form_->get(name, "");
            } catch(const Poco::Exception& e) {
                WARNING_LOG(
                    "Reading HTML form with Poco failed with exception ",
                    "(defaulting to empty): ", e.displayText()
                );
            }
        }
        return "";
    }

    optional<string> getBasicAuthCredentials() {
        REQUIRE(request_ != nullptr);
        PANIC("TODO: not implemented");
        return {};
    }

    void sendResponse(
        int status,
        string contentType,
        uint64_t contentLength,
        function<void(ostream&)> body,
        bool noCache,
        vector<pair<string, string>> extraHeaders
    ) {
        REQUIRE(request_ != nullptr);
        request_ = nullptr;

        try {
            responderPromise_.set_value([
                status,
                contentType{move(contentType)},
                contentLength,
                body{move(body)},
                noCache,
                extraHeaders{move(extraHeaders)}
            ](Poco::Net::HTTPServerResponse& response) {
                response.add("Content-Type", contentType);
                response.setContentLength64(contentLength);
                if(noCache) {
                    response.add("Cache-Control", "no-cache, no-store, must-revalidate");
                    response.add("Pragma", "no-cache");
                    response.add("Expires", "0");
                }
                for(const pair<string, string>& header : extraHeaders) {
                    response.add(header.first, header.second);
                }
                response.setStatus((Poco::Net::HTTPResponse::HTTPStatus)status);
                body(response.send());
            });
        } catch(const future_error& e) {
            PANIC(
                "Sending HTTP response to background thread failed with ",
                "std::future error ", e.code(), ": ", e.what()
            );
        }
    }

    void sendTextResponse(
        int status,
        string text,
        bool noCache,
        vector<pair<string, string>> extraHeaders
    ) {
        REQUIRE(request_ != nullptr);

        uint64_t contentLength = text.size();
        sendResponse(
            status,
            "text/plain; charset=UTF-8",
            contentLength,
            [text{move(text)}](ostream& out) {
                out << text;
            },
            noCache,
            move(extraHeaders)
        );
    }

private:
    AliveToken aliveToken_;

    // nullptr after the response has been sent.
    Poco::Net::HTTPServerRequest* request_;

    string method_;
    string path_;
    string userAgent_;

    bool formParsed_;
    optional<Poco::Net::HTMLForm> form_;

    promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise_;
};

HTTPRequest::HTTPRequest(CKey, unique_ptr<Impl> impl)
    : impl_(move(impl))
{
    REQUIRE(impl_);
}

string HTTPRequest::method() {
    REQUIRE_API_THREAD();
    return impl_->method();
}

string HTTPRequest::path() {
    REQUIRE_API_THREAD();
    return impl_->path();
}

string HTTPRequest::userAgent() {
    REQUIRE_API_THREAD();
    return impl_->userAgent();
}

string HTTPRequest::getFormParam(string name) {
    REQUIRE_API_THREAD();
    return impl_->getFormParam(move(name));
}

optional<string> HTTPRequest::getBasicAuthCredentials() {
    REQUIRE_API_THREAD();
    return impl_->getBasicAuthCredentials();
}

void HTTPRequest::sendResponse(
    int status,
    string contentType,
    uint64_t contentLength,
    function<void(ostream&)> body,
    bool noCache,
    vector<pair<string, string>> extraHeaders
) {
    REQUIRE_API_THREAD();
    impl_->sendResponse(
        status,
        move(contentType),
        contentLength,
        move(body),
        noCache,
        move(extraHeaders)
    );
}

void HTTPRequest::sendTextResponse(
    int status,
    string text,
    bool noCache,
    vector<pair<string, string>> extraHeaders
) {
    REQUIRE_API_THREAD();
    impl_->sendTextResponse(
        status,
        move(text),
        noCache,
        move(extraHeaders)
    );
}

namespace http_ {

class HTTPRequestHandler : public Poco::Net::HTTPRequestHandler {
public:
    HTTPRequestHandler(
        weak_ptr<HTTPServerEventHandler> eventHandler,
        shared_ptr<TaskQueue> taskQueue,
        AliveToken aliveToken
    )
        : aliveToken_(aliveToken),
          eventHandler_(eventHandler),
          taskQueue_(taskQueue)
    {}

    virtual void handleRequest(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response
    ) override {
        ActiveTaskQueueLock activeTaskQueueLock(taskQueue_);

        promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise;
        future<function<void(Poco::Net::HTTPServerResponse&)>> responderFuture =
            responderPromise.get_future();

        {
            shared_ptr<HTTPRequest> reqObj = HTTPRequest::create(
                make_unique<HTTPRequest::Impl>(
                    request,
                    move(responderPromise),
                    aliveToken_
                )
            );
            postTask(
                eventHandler_,
                &HTTPServerEventHandler::onHTTPServerRequest,
                reqObj
            );
        }

        function<void(Poco::Net::HTTPServerResponse&)> responder;
        try {
            responder = responderFuture.get();
        } catch(const future_error& e) {
            PANIC(
                "Receiving HTTP response object from the handler failed with ",
                "std::future error ", e.code(), ": ", e.what()
            );
        }

        responder(response);
    }

private:
    AliveToken aliveToken_;
    weak_ptr<HTTPServerEventHandler> eventHandler_;
    shared_ptr<TaskQueue> taskQueue_;
};

}

namespace {

using http_::HTTPRequestHandler;

class HTTPRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
    HTTPRequestHandlerFactory(
        weak_ptr<HTTPServerEventHandler> eventHandler,
        shared_ptr<TaskQueue> taskQueue,
        AliveToken aliveToken
    )
        : aliveToken_(aliveToken),
          eventHandler_(eventHandler),
          taskQueue_(taskQueue)
    {}

    virtual Poco::Net::HTTPRequestHandler* createRequestHandler(
        const Poco::Net::HTTPServerRequest& request
    ) override {
        return new HTTPRequestHandler(eventHandler_, taskQueue_, aliveToken_);
    }

private:
    AliveToken aliveToken_;
    weak_ptr<HTTPServerEventHandler> eventHandler_;
    shared_ptr<TaskQueue> taskQueue_;
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
