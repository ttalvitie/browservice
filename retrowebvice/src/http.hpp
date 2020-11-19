#pragma once

#include "common.hpp"

namespace retrowebvice {

class Context;
namespace http_ {
    class HTTPRequestHandler;
}

// Information about a single request. The response should be sent by calling
// one of the send* functions exactly once. If no response is given, a internal
// server error response is sent upon object destruction and a warning is
// logged.
class HTTPRequest {
public:
    HTTPRequest(const HTTPRequest&) = delete;
    HTTPRequest(HTTPRequest&&) = delete;
    HTTPRequest& operator=(const HTTPRequest&) = delete;
    HTTPRequest& operator=(HTTPRequest&&) = delete;

    string method();
    string path();
    string userAgent();

    string getFormParam(string name);

    optional<string> getBasicAuthCredentials();

    // In case of HTTP server internal errors or server shutdown, writing to the
    // ostream given to the body function may throw an exception or the body
    // function may not even be called. If it is called and writes are
    // successful, the given content length should match the number of bytes
    // written.
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
    class Impl;
    HTTPRequest(unique_ptr<Impl> impl);
    unique_ptr<Impl> impl_;
    friend class http_::HTTPRequestHandler;
};

class SocketAddress {
public:
    // Create socket address from string representation of type "ADDRESS:PORT",
    // such as "127.0.0.1:8080". Returns empty optional if parsing the
    // representation failed.
    static optional<SocketAddress> parse(string repr);

private:
    SocketAddress();

    struct Impl;
    shared_ptr<Impl> impl_;

    friend class HTTPServer;
};

class HTTPServer {
public:
    // Start HTTP server listening to given address, returning immediately. The
    // requests are handled by ctx.handleHTTPRequest in separate HTTP server
    // threads. Must be shut down by calling shutdown and waiting for completion
    // prior to destruction.
    HTTPServer(
        Context& ctx,
        SocketAddress listenAddr,
        int maxThreads
    );
    ~HTTPServer();

    HTTPServer(const HTTPServer&) = delete;
    HTTPServer(HTTPServer&&) = delete;
    HTTPServer& operator=(const HTTPServer&) = delete;
    HTTPServer& operator=(HTTPServer&&) = delete;

    // Start shutting down a running server (may only be called once). When the
    // shutdown is complete, completionCallback will be called once in a
    // background thread. After this, it is guaranteed that all request handler
    // calls (including calls to the body function supplied in
    // HTTPRequest::sendResponse) have terminated, no more calls will be made
    // and the HTTPServer object may be destructed.
    void asyncShutdown(function<void()> completionCallback);

private:
    Context& ctx_;
    enum {Running, ShutdownPending, ShutdownComplete} state_;

    struct PocoHTTPServer;
    unique_ptr<PocoHTTPServer> pocoHTTPServer_;
};

}
