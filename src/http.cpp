#include "http.hpp"

#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>

class HTTPRequest::Impl {
public:
    Impl(
        Poco::Net::HTTPServerRequest& request,
        promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise
    )
        : request_(request),
          responderPromise_(move(responderPromise)),
          responseSent_(false)
    {}

    ~Impl() {
        if(!responseSent_) {
            LOG(WARNING) << "HTTP response not provided, sending internal server error";
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
        CHECK(!responseSent_);
        return request_.getMethod();
    }
    string path() {
        CHECK(!responseSent_);
        return request_.getURI();
    }
    string userAgent() {
        CHECK(!responseSent_);
        return request_.get("User-Agent", "");
    }

    string getFormParam(string name) {
        CHECK(!responseSent_);
        if(!form_) {
            if(method() == "POST") {
                form_.emplace(request_, request_.stream());
            } else {
                form_.emplace();
            }
        }
        return form_->get(name, "");
    }

    void sendResponse(
        int status,
        string contentType,
        uint64_t contentLength,
        function<void(ostream&)> body,
        bool noCache,
        vector<pair<string, string>> extraHeaders
    ) {
        CHECK(!responseSent_);
        responseSent_ = true;
        responderPromise_.set_value(
            [
                status,
                contentType{move(contentType)},
                contentLength,
                body,
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
            }
        );
    }

    void sendTextResponse(
        int status,
        string text,
        bool noCache,
        vector<pair<string, string>> extraHeaders
    ) {
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
    Poco::Net::HTTPServerRequest& request_;
    optional<Poco::Net::HTMLForm> form_;
    promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise_;
    bool responseSent_;
};

namespace http_ {

class HTTPRequestHandler : public Poco::Net::HTTPRequestHandler {
public:
    HTTPRequestHandler(weak_ptr<HTTPServerEventHandler> eventHandler)
        : eventHandler_(eventHandler)
    {}

    virtual void handleRequest(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response
    ) override {
        promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise;
        future<function<void(Poco::Net::HTTPServerResponse&)>> responderFuture =
            responderPromise.get_future();
        
        {
            shared_ptr<HTTPRequest> reqObj = HTTPRequest::create(
                make_unique<HTTPRequest::Impl>(request, move(responderPromise))
            );
            postTask(
                eventHandler_,
                &HTTPServerEventHandler::onHTTPServerRequest,
                reqObj
            );
        }

        std::function<void(Poco::Net::HTTPServerResponse&)> responder = responderFuture.get();
        responder(response);
    }

private:
    weak_ptr<HTTPServerEventHandler> eventHandler_;
};

class HTTPRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
    HTTPRequestHandlerFactory(weak_ptr<HTTPServerEventHandler> eventHandler)
        : eventHandler_(eventHandler)
    {}

    virtual Poco::Net::HTTPRequestHandler* createRequestHandler(
        const Poco::Net::HTTPServerRequest& request
    ) override {
        return new HTTPRequestHandler(eventHandler_);
    }

private:
    weak_ptr<HTTPServerEventHandler> eventHandler_;
};

}
namespace { using namespace http_; }

HTTPRequest::HTTPRequest(CKey, unique_ptr<Impl> impl)
    : impl_(move(impl))
{}

string HTTPRequest::method() {
    requireUIThread();
    return impl_->method();
}

string HTTPRequest::path() {
    requireUIThread();
    return impl_->path();
}

string HTTPRequest::userAgent() {
    requireUIThread();
    return impl_->userAgent();
}

string HTTPRequest::getFormParam(string name) {
    requireUIThread();
    return impl_->getFormParam(name);
}

void HTTPRequest::sendResponse(
    int status,
    string contentType,
    uint64_t contentLength,
    function<void(ostream&)> body,
    bool noCache,
    vector<pair<string, string>> extraHeaders
) {
    requireUIThread();
    impl_->sendResponse(
        status, contentType, contentLength, body, noCache, move(extraHeaders)
    );
}

void HTTPRequest::sendTextResponse(
    int status,
    string text,
    bool noCache,
    vector<pair<string, string>> extraHeaders
) {
    requireUIThread();
    impl_->sendTextResponse(status, move(text), noCache, move(extraHeaders));
}

class HTTPServer::Impl : public enable_shared_from_this<Impl> {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(CKey,
        weak_ptr<HTTPServerEventHandler> eventHandler,
        const std::string& listenSockAddr
    )
        : eventHandler_(eventHandler),
          state_(Running),
          socketAddress_(listenSockAddr),
          serverSocket_(socketAddress_),
          httpServer_(
              new HTTPRequestHandlerFactory(eventHandler),
              threadPool_,
              serverSocket_,
              new Poco::Net::HTTPServerParams()
          )
    {
        LOG(INFO) << "HTTP server listening to " << listenSockAddr;
        httpServer_.start();
    }
    ~Impl() {
        CHECK(state_ == ShutdownComplete);
    }

    void shutdown() {
        requireUIThread();
        
        if(state_ != Running) {
            return;
        }
        LOG(INFO) << "Shutting down HTTP server";
        state_ = ShutdownPending;
        
        shared_ptr<Impl> self = shared_from_this();
        thread stopThread([self{move(self)}]() {
            // Do not accept new connections
            self->httpServer_.stop();

            // 1s grace time for current connections before abort
            for(int i = 0; i < 10; ++i) {
                if(self->httpServer_.currentConnections() == 0) {
                    break;
                }
                sleep_for(milliseconds(100));
            }
            self->httpServer_.stopAll(true);

            postTask([self{move(self)}]() {
                CHECK(self->state_ == ShutdownPending);
                self->state_ = ShutdownComplete;
                LOG(INFO) << "HTTP server shutdown complete";
                if(auto eventHandler = self->eventHandler_.lock()) {
                    eventHandler->onHTTPServerShutdownComplete();
                }
            });
        });
        stopThread.detach();
    }

    bool isShutdownComplete() {
        requireUIThread();
        return state_ == ShutdownComplete;
    }

private:
    weak_ptr<HTTPServerEventHandler> eventHandler_;

    enum {Running, ShutdownPending, ShutdownComplete} state_;

    Poco::ThreadPool threadPool_;
    Poco::Net::SocketAddress socketAddress_;
    Poco::Net::ServerSocket serverSocket_;
    Poco::Net::HTTPServer httpServer_;
};

HTTPServer::HTTPServer(CKey,
    weak_ptr<HTTPServerEventHandler> eventHandler,
    const std::string& listenSockAddr
) {
    requireUIThread();
    impl_ = Impl::create(eventHandler, listenSockAddr);
}

HTTPServer::~HTTPServer() {
    postTask(impl_, &Impl::shutdown);
}

void HTTPServer::shutdown() {
    impl_->shutdown();
}

bool HTTPServer::isShutdownComplete() {
    return impl_->isShutdownComplete();
}
