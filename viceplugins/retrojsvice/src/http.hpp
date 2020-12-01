#pragma once

#include "common.hpp"

namespace retrojsvice {

class SocketAddress {
public:
    // Create socket address from string representation of type "ADDRESS:PORT",
    // such as "127.0.0.1:8080". Returns an empty optional if parsing the
    // representation failed.
    static optional<SocketAddress> parse(string repr);

private:
    SocketAddress();

    struct Impl;
    shared_ptr<Impl> impl_;

    friend class HTTPServer;
    friend ostream& operator<<(ostream& out, SocketAddress addr);
};

ostream& operator<<(ostream& out, SocketAddress addr);

class HTTPServerEventHandler {
public:
    virtual void onHTTPServerShutdownComplete() = 0;
};

// HTTP server that delegates requests to be handled by given event handler
// through onHTTPServerRequest. Before destruction, call shutdown and wait for
// onHTTPServerShutdownComplete event.
class HTTPServer {
SHARED_ONLY_CLASS(HTTPServer);
public:
    HTTPServer(CKey,
        weak_ptr<HTTPServerEventHandler> eventHandler,
        SocketAddress listenAddr,
        int maxThreads
    );
    ~HTTPServer();

    void shutdown();

private:
    class Impl;
    shared_ptr<Impl> impl_;
};

}
