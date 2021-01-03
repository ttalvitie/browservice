#pragma once

#include "session.hpp"
#include "vice.hpp"

class ServerEventHandler {
public:
    virtual void onServerShutdownComplete() = 0;
};

// The root object for the whole browser proxy server, handling multiple
// browser sessions. Before quitting CEF message loop, call shutdown and wait
// for onServerShutdownComplete event.
class Server :
    public ViceContextEventHandler,
    public SessionEventHandler,
    public enable_shared_from_this<Server>
{
SHARED_ONLY_CLASS(Server);
public:
    Server(CKey,
        weak_ptr<ServerEventHandler> eventHandler,
        shared_ptr<ViceContext> viceCtx
    );

    // Shutdown the server if it is not already shut down
    void shutdown();

    // ViceContextEventHandler:
    virtual uint64_t onViceContextCreateWindowRequest(string& reason) override;
    virtual void onViceContextCloseWindow(uint64_t window) override;
    virtual void onViceContextFetchWindowImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> putImage
    ) override;
    virtual void onViceContextShutdownComplete() override;

    // SessionEventHandler:
    virtual void onSessionClosing(uint64_t id) override;
    virtual void onSessionClosed(uint64_t id) override;
    virtual void onSessionViewImageChanged(uint64_t id) override;

private:
    void afterConstruct_(shared_ptr<Server> self);

    void checkSessionsEmpty_();

    weak_ptr<ServerEventHandler> eventHandler_;

    uint64_t nextSessionID_;
    enum {Running, WaitSessions, WaitViceContext, ShutdownComplete} state_;

    shared_ptr<ViceContext> viceCtx_;
    map<uint64_t, shared_ptr<Session>> sessions_;
};
