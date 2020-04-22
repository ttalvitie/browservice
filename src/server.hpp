#pragma once

#include "http.hpp"
#include "session.hpp"

class ServerEventHandler {
public:
    virtual void onServerShutdownComplete() = 0;
};

// The root object for the whole browser proxy server, handling multiple
// clients. Before quitting CEF message loop, call shutdown and wait for
// onServerShutdownComplete event.
class Server : public HTTPServerEventHandler {
SHARED_ONLY_CLASS(Server);
public:
    Server(CKey, weak_ptr<ServerEventHandler> eventHandler);

    void shutdown();

    // HTTPServerEventHandler:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request);
    virtual void onHTTPServerShutdownComplete() override;

private:
    void afterConstruct_(shared_ptr<Server> self);

    weak_ptr<ServerEventHandler> eventHandler_;
    shared_ptr<HTTPServer> httpServer_;
    map<uint64_t, shared_ptr<Session>> sessions_;
};
