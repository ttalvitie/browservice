#pragma once

#include "http.hpp"
#include "session.hpp"

class ServerEventHandler {
public:
    virtual void onServerShutdownComplete() = 0;
};

// The root object for the whole browser proxy server, handling multiple
// browser sessions. Before quitting CEF message loop, call shutdown and wait
// for onServerShutdownComplete event.
class Server :
    public HTTPServerEventHandler,
    public SessionEventHandler,
    public enable_shared_from_this<Server>
{
SHARED_ONLY_CLASS(Server);
public:
    Server(CKey, weak_ptr<ServerEventHandler> eventHandler);

    // Shutdown the server if it is not already shut down
    void shutdown();

    // HTTPServerEventHandler:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request) override;
    virtual void onHTTPServerShutdownComplete() override;

    // SessionEventHandler:
    virtual void onSessionClosed(uint64_t id) override;
    virtual bool onIsServerFullQuery() override;
    virtual void onPopupSessionOpen(shared_ptr<Session> session) override;

private:
    void afterConstruct_(shared_ptr<Server> self);

    void handleClipboardRequest_(shared_ptr<HTTPRequest> request);

    void checkShutdownStatus_();

    weak_ptr<ServerEventHandler> eventHandler_;

    enum {Running, ShutdownPending, ShutdownComplete} state_;

    shared_ptr<HTTPServer> httpServer_;
    map<uint64_t, shared_ptr<Session>> sessions_;
};
