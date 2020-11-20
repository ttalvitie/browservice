#include "http.hpp"

#include "context.hpp"

#include <Poco/Base64Decoder.h>

#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

namespace retrowebvice {

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
    Impl(
        Context& ctx,
        AliveToken aliveToken,
        Poco::Net::HTTPServerRequest& request
    )
        : ctx_(ctx),
          aliveToken_(aliveToken),
          request_(request),
          method_(request.getMethod()),
          path_(request.getURI()),
          userAgent_(request.get("User-Agent", "")),
          formParsed_(false),
          responseSent_(false)
    {}

    string method() {
        return method_;
    }
    string path() {
        return path_;
    }
    string userAgent() {
        return userAgent_;
    }

    string getFormParam(string name) {
        if(!formParsed_) {
            formParsed_ = true;
            try {
                if(method() == "POST") {
                    form_.emplace(request_, request_.stream());
                } else {
                    form_.emplace();
                }
            } catch(const Poco::Exception& e) {
                ctx_.WARNING_LOG(
                    "Parsing HTML form with Poco failed with exception ",
                    "(defaulting to empty): ", e.displayText()
                );
            }
        }

        if(form_) {
            try {
                return form_->get(name, "");
            } catch(const Poco::Exception& e) {
                ctx_.WARNING_LOG(
                    "Reading HTML form with Poco failed with exception ",
                    "(defaulting to empty): ", e.displayText()
                );
            }
        }
        return "";
    }

    optional<string> getBasicAuthCredentials() {
        optional<string> empty;

        string scheme, authInfoBase64;

        try {
            if(!request_.hasCredentials()) {
                return empty;
            }
            request_.getCredentials(scheme, authInfoBase64);
        } catch(const Poco::Exception& e) {
            ctx_.WARNING_LOG(
                "Reading HTTP auth credentials with Poco failed with exception ",
                "(defaulting to none): ", e.displayText()
            );
            return empty;
        }

        for(char& c : scheme) {
            c = tolower(c);
        }
        if(scheme != "basic") {
            return empty;
        }

        string authInfo;

        try {
            stringstream authInfoBase64SS(authInfoBase64);
            Poco::Base64Decoder decoder(authInfoBase64SS);

            size_t BufSize = 1024;
            char buf[BufSize];
            while(decoder.good()) {
                decoder.read(buf, BufSize);
                if(decoder.bad()) {
                    return empty;
                }
                authInfo.append(buf, decoder.gcount());
            }
        } catch(const Poco::Exception& e) {
            ctx_.WARNING_LOG(
                "Parsing HTTP basic auth credentials with Poco failed with exception ",
                "(defaulting to none): ", e.displayText()
            );
            return empty;
        }

        return authInfo;
    }

    void sendResponse(
        int status,
        string contentType,
        uint64_t contentLength,
        function<void(ostream&)> body,
        bool noCache,
        vector<pair<string, string>> extraHeaders
    ) {
        ctx_.REQUIRE(!responseSent_);

        responseSent_ = true;
        responder_ = [
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
        };
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
    Context& ctx_;
    AliveToken aliveToken_;

    Poco::Net::HTTPServerRequest& request_;

    string method_;
    string path_;
    string userAgent_;

    bool formParsed_;
    optional<Poco::Net::HTMLForm> form_;

    bool responseSent_;
    function<void(Poco::Net::HTTPServerResponse&)> responder_;

    friend class http_::HTTPRequestHandler;
};

string HTTPRequest::method() {
    return impl_->method();
}
string HTTPRequest::path() {
    return impl_->path();
}
string HTTPRequest::userAgent() {
    return impl_->userAgent();
}
string HTTPRequest::getFormParam(string name) {
    return impl_->getFormParam(move(name));
}
optional<string> HTTPRequest::getBasicAuthCredentials() {
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
    return impl_->sendResponse(
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
    return impl_->sendTextResponse(
        status,
        move(text),
        noCache,
        move(extraHeaders)
    );
}
HTTPRequest::HTTPRequest(unique_ptr<Impl> impl)
    : impl_(move(impl))
{}

namespace http_ {

class HTTPRequestHandler : public Poco::Net::HTTPRequestHandler {
public:
    HTTPRequestHandler(Context& ctx, AliveToken aliveToken)
        : ctx_(ctx),
          aliveToken_(aliveToken)
    {}

    virtual void handleRequest(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response
    ) override {
        HTTPRequest httpRequest(
            make_unique<HTTPRequest::Impl>(ctx_, aliveToken_, request)
        );

        ctx_.handleHTTPRequest(httpRequest);

        if(!httpRequest.impl_->responseSent_) {
            ctx_.WARNING_LOG("HTTP response not provided, sending internal server error");
            httpRequest.sendTextResponse(
                500,
                "ERROR: Request handling failure\n",
                true,
                {}
            );
        }

        ctx_.REQUIRE(httpRequest.impl_->responseSent_);
        httpRequest.impl_->responder_(response);
    }

private:
    Context& ctx_;
    AliveToken aliveToken_;
};

class HTTPRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
public:
    HTTPRequestHandlerFactory(Context& ctx, AliveToken aliveToken)
        : ctx_(ctx),
          aliveToken_(aliveToken)
    {}

    virtual Poco::Net::HTTPRequestHandler* createRequestHandler(
        const Poco::Net::HTTPServerRequest& request
    ) override {
        return new HTTPRequestHandler(ctx_, aliveToken_);
    }

private:
    Context& ctx_;
    AliveToken aliveToken_;
};

}

namespace {
    using namespace http_;
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

struct HTTPServer::PocoHTTPServer {
    Poco::ThreadPool threadPool;
    Poco::Net::SocketAddress socketAddress;
    Poco::Net::ServerSocket serverSocket;
    Poco::Net::HTTPServer httpServer;
    AliveToken aliveToken;

    PocoHTTPServer(
        SocketAddress listenAddr,
        int maxThreads,
        unique_ptr<HTTPRequestHandlerFactory> requestHandlerFactory,
        AliveToken aliveToken
    )
        : threadPool(1, maxThreads),
          socketAddress(listenAddr.impl_->addr),
          serverSocket(socketAddress),
          httpServer(
              requestHandlerFactory.release(),
              threadPool,
              serverSocket,
              new Poco::Net::HTTPServerParams()
          ),
          aliveToken(aliveToken)
    {}
};

HTTPServer::HTTPServer(
    Context& ctx,
    SocketAddress listenAddr,
    int maxThreads
)
    : ctx_(ctx),
      state_(Running)
{
    ctx.INFO_LOG(
        "Starting HTTP server (listen address: ",
        listenAddr.impl_->addrStr, ")"
    );

    AliveToken aliveToken = AliveToken::create();
    unique_ptr<HTTPRequestHandlerFactory> requestHandlerFactory =
        make_unique<HTTPRequestHandlerFactory>(ctx, aliveToken);

    try {
        pocoHTTPServer_ = make_unique<PocoHTTPServer>(
            listenAddr,
            maxThreads,
            move(requestHandlerFactory),
            aliveToken
        );
        pocoHTTPServer_->httpServer.start();
    } catch(const Poco::Exception& e) {
        ctx.PANIC("Starting Poco HTTP server failed with exception: ", e.displayText());
    }

    ctx.INFO_LOG("HTTP server started successfully");
}

HTTPServer::~HTTPServer() {
    ctx_.REQUIRE(state_ == ShutdownComplete);
}

void HTTPServer::asyncShutdown(function<void()> completionCallback) {
    ctx_.REQUIRE(state_ == Running);
    state_ = ShutdownPending;

    thread([this, completionCallback]() {
        ctx_.INFO_LOG("Shutting down HTTP server");

        AliveTokenWatcher watcher(pocoHTTPServer_->aliveToken);

        try {
            // Do not accept new connections
            pocoHTTPServer_->httpServer.stop();

            // 1s grace time for current connections before abort
            for(int i = 0; i < 10; ++i) {
                if(pocoHTTPServer_->httpServer.currentConnections() == 0) {
                    break;
                }
                sleep_for(milliseconds(100));
            }
            pocoHTTPServer_->httpServer.stopAll(true);
            pocoHTTPServer_.reset();
        } catch(const Poco::Exception& e) {
            ctx_.PANIC(
                "Shutting down Poco HTTP server failed with exception: ",
                e.displayText()
            );
        }

        // Just to be safe, use our alive token to wait for possibly lingering
        // Poco HTTP server background threads to actually shut down so that we
        // can be sure that we won't get any calls to the HTTP request handler
        // after shutdown.
        while(watcher.isTokenAlive()) {
            sleep_for(milliseconds(100));
        }

        ctx_.INFO_LOG("HTTP server shutdown complete");

        state_ = ShutdownComplete;
        completionCallback();
    }).detach();
}

}
