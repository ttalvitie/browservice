#pragma once

#include "common.hpp"

namespace http_ {
    class HTTPRequestHandler;
}

// Information about a single request. The response should be sent by calling
// one of the send* functions exactly once. If no response is given, a internal
// server error response is sent upon object destruction and a warning is
// logged. No other member functions may be called after sending the response.
class HTTPRequest {
SHARED_ONLY_CLASS(HTTPRequest);
private:
    class Impl;

public:
    HTTPRequest(CKey, unique_ptr<Impl> impl);

    string method();
    string path();
    string userAgent();

    string getFormParam(string name);

    // The body function may be called from a different thread. The given
    // content length should match the number of bytes written by body.
    void sendResponse(
        int status,
        string contentType,
        uint64_t contentLength,
        function<void(ostream&)> body,
        bool noCache = true,
        vector<pair<string, string>> extraHeaders = {}
    );

    void sendTextResponse(
        int status,
        string text,
        bool noCache = true,
        vector<pair<string, string>> extraHeaders = {}
    );

    template <typename Data>
    void sendHTMLResponse(
        int status,
        void (*writer)(ostream&, const Data&),
        const Data& data,
        bool noCache = true,
        vector<pair<string, string>> extraHeaders = {}
    ) {
        stringstream htmlSS;
        writer(htmlSS, data);

        string html = htmlSS.str();
        uint64_t contentLength = html.size();

        sendResponse(
            status,
            "text/html; charset=UTF-8",
            contentLength,
            [html{move(html)}](ostream& out) {
                out << html;
            },
            noCache,
            move(extraHeaders)
        );
    }

private:
    unique_ptr<Impl> impl_;

    friend class http_::HTTPRequestHandler;
};

class HTTPServerEventHandler {
public:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request) = 0;
    virtual void onHTTPServerShutdownComplete() = 0;
};

// HTTP server that delegates requests to be handled by given event handler
// through onHTTPServerRequest. Before quitting CEF message loop, call shutdown
// and wait for onHTTPServerShutdownComplete event.
class HTTPServer {
SHARED_ONLY_CLASS(HTTPServer);
public:
    HTTPServer(CKey,
        weak_ptr<HTTPServerEventHandler> eventHandler,
        const std::string& listenSockAddr
    );
    ~HTTPServer();

    void shutdown();
    bool isShutdownComplete();

private:
    class Impl;
    shared_ptr<Impl> impl_;
};
