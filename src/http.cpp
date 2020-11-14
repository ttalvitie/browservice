#include "http.hpp"

#include "globals.hpp"

#include "include/cef_parser.h"

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
        REQUIRE(!responseSent_);
        return request_.getMethod();
    }
    string path() {
        REQUIRE(!responseSent_);
        return request_.getURI();
    }
    string userAgent() {
        REQUIRE(!responseSent_);
        return request_.get("User-Agent", "");
    }

    string getFormParam(string name) {
        REQUIRE(!responseSent_);
        if(!form_) {
            if(method() == "POST") {
                form_.emplace(request_, request_.stream());
            } else {
                form_.emplace();
            }
        }
        return form_->get(name, "");
    }

    optional<string> getBasicAuthCredentials() {
        REQUIRE(!responseSent_);
        optional<string> empty;

        if(!request_.hasCredentials()) {
            return empty;
        }

        string scheme, authInfo;
        request_.getCredentials(scheme, authInfo);

        for(char& c : scheme) {
            c = tolower(c);
        }
        if(scheme != "basic") {
            return empty;
        }

        CefRefPtr<CefBinaryValue> bin = CefBase64Decode(authInfo);
        if(!bin) {
            return empty;
        }

        vector<char> buf(bin->GetSize());
        bin->GetData(buf.data(), buf.size(), 0);

        return string(buf.begin(), buf.end());
    }

    void sendResponse(
        int status,
        string contentType,
        uint64_t contentLength,
        function<void(ostream&)> body,
        bool noCache,
        vector<pair<string, string>> extraHeaders
    ) {
        REQUIRE(!responseSent_);
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
    REQUIRE_UI_THREAD();
    return impl_->method();
}

string HTTPRequest::path() {
    REQUIRE_UI_THREAD();
    return impl_->path();
}

string HTTPRequest::userAgent() {
    REQUIRE_UI_THREAD();
    return impl_->userAgent();
}

string HTTPRequest::getFormParam(string name) {
    REQUIRE_UI_THREAD();
    return impl_->getFormParam(name);
}

optional<string> HTTPRequest::getBasicAuthCredentials() {
    REQUIRE_UI_THREAD();
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
    REQUIRE_UI_THREAD();
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
    REQUIRE_UI_THREAD();
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
          threadPool_(2, 2 * globals->config->sessionLimit + 16),
          socketAddress_(listenSockAddr),
          serverSocket_(socketAddress_),
          httpServer_(
              new HTTPRequestHandlerFactory(eventHandler),
              threadPool_,
              serverSocket_,
              new Poco::Net::HTTPServerParams()
          )
    {
        INFO_LOG("HTTP server listening to ", listenSockAddr);
        httpServer_.start();
    }
    ~Impl() {
        REQUIRE(state_ == ShutdownComplete);
    }

    void shutdown() {
        REQUIRE_UI_THREAD();
        
        if(state_ != Running) {
            return;
        }
        INFO_LOG("Shutting down HTTP server");
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
                REQUIRE(self->state_ == ShutdownPending);
                self->state_ = ShutdownComplete;
                INFO_LOG("HTTP server shutdown complete");
                if(auto eventHandler = self->eventHandler_.lock()) {
                    eventHandler->onHTTPServerShutdownComplete();
                }
            });
        });
        stopThread.detach();
    }

    bool isShutdownComplete() {
        REQUIRE_UI_THREAD();
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
    REQUIRE_UI_THREAD();
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
