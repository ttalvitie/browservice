#include "http.hpp"

#include "task_queue.hpp"
#include "upload.hpp"

#include <Poco/Base64Decoder.h>

#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/PartHandler.h>
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
        unique_ptr<Poco::Net::HTMLForm> form,
        map<string, shared_ptr<FileUpload>> files,
        promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise,
        AliveToken aliveToken
    )
        : aliveToken_(aliveToken),
          request_(&request),
          method_(request.getMethod()),
          path_(request.getURI()),
          userAgent_(request.get("User-Agent", "")),
          form_(move(form)),
          files_(move(files)),
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

    shared_ptr<FileUpload> getFormFile(string name) {
        REQUIRE(request_ != nullptr);

        auto it = files_.find(name);
        if(it == files_.end()) {
            shared_ptr<FileUpload> empty;
            return empty;
        } else {
            return it->second;
        }
    }

    optional<string> getBasicAuthCredentials() {
        REQUIRE(request_ != nullptr);

        optional<string> empty;

        string scheme, authInfoBase64;

        try {
            if(!request_->hasCredentials()) {
                return empty;
            }
            request_->getCredentials(scheme, authInfoBase64);
        } catch(const Poco::Exception& e) {
            WARNING_LOG(
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

            const size_t BufSize = 1024;
            char buf[BufSize];
            while(decoder.good()) {
                decoder.read(buf, BufSize);
                if(decoder.bad()) {
                    return empty;
                }
                authInfo.append(buf, (size_t)decoder.gcount());
            }
        } catch(const Poco::Exception& e) {
            WARNING_LOG(
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

    unique_ptr<Poco::Net::HTMLForm> form_;
    map<string, shared_ptr<FileUpload>> files_;

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

shared_ptr<FileUpload> HTTPRequest::getFormFile(string name) {
    REQUIRE_API_THREAD();
    return impl_->getFormFile(move(name));
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

struct FormPartHandler : public Poco::Net::PartHandler {
    virtual void handlePart(
        const Poco::Net::MessageHeader& header,
        istream& dataStream
    ) override {
        string dispStr = header.get("Content-Disposition", "");
        string dispVal;
        Poco::Net::NameValueCollection disp;
        header.splitParameters(dispStr, dispVal, disp);

        for(char& c : dispVal) {
            c = tolower(c);
        }
        if(dispVal != "form-data") {
            return;
        }

        string name = disp.get("name", "");
        string filename = disp.get("filename", "");

        if(filename.empty()) {
            return;
        }

        REQUIRE(storage);
        shared_ptr<FileUpload> file = storage->upload(filename, dataStream);
        if(file) {
            files->emplace(name, file);
        }
    }

    shared_ptr<UploadStorage> storage;
    map<string, shared_ptr<FileUpload>>* files;
};

class HTTPRequestHandler : public Poco::Net::HTTPRequestHandler {
public:
    HTTPRequestHandler(
        weak_ptr<HTTPServerEventHandler> eventHandler,
        shared_ptr<TaskQueue> taskQueue,
        shared_ptr<UploadStorage> uploadStorage,
        AliveToken aliveToken
    )
        : aliveToken_(aliveToken),
          eventHandler_(eventHandler),
          taskQueue_(taskQueue),
          uploadStorage_(uploadStorage)
    {}

    virtual void handleRequest(
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response
    ) override {
        ActiveTaskQueueLock activeTaskQueueLock(taskQueue_);

        unique_ptr<Poco::Net::HTMLForm> form;
        map<string, shared_ptr<FileUpload>> files;
        try {
            if(request.getMethod() == "POST") {
                FormPartHandler partHandler;
                partHandler.storage = uploadStorage_;
                partHandler.files = &files;
                form = make_unique<Poco::Net::HTMLForm>(
                    request, request.stream(), partHandler
                );
            }
        } catch(const Poco::Exception& e) {
            WARNING_LOG(
                "Parsing HTML form with Poco failed with exception ",
                "(defaulting to empty): ", e.displayText()
            );
        }

        promise<function<void(Poco::Net::HTTPServerResponse&)>> responderPromise;
        future<function<void(Poco::Net::HTTPServerResponse&)>> responderFuture =
            responderPromise.get_future();

        {
            shared_ptr<HTTPRequest> reqObj = HTTPRequest::create(
                make_unique<HTTPRequest::Impl>(
                    request,
                    move(form),
                    move(files),
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
    shared_ptr<UploadStorage> uploadStorage_;
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
    {
        uploadStorage_ = UploadStorage::create();
    }

    virtual Poco::Net::HTTPRequestHandler* createRequestHandler(
        const Poco::Net::HTTPServerRequest& request
    ) override {
        return new HTTPRequestHandler(
            eventHandler_, taskQueue_, uploadStorage_, aliveToken_
        );
    }

private:
    AliveToken aliveToken_;
    weak_ptr<HTTPServerEventHandler> eventHandler_;
    shared_ptr<TaskQueue> taskQueue_;
    shared_ptr<UploadStorage> uploadStorage_;
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
