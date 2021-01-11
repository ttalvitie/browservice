#pragma once

#include "window.hpp"
#include "vice.hpp"

namespace browservice {

class ServerEventHandler {
public:
    virtual void onServerShutdownComplete() = 0;
};

// The root object for the whole browser proxy server, handling multiple
// browser windows. Before quitting CEF message loop, call shutdown and wait
// for onServerShutdownComplete event.
class Server :
    public ViceContextEventHandler,
    public WindowEventHandler,
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
    virtual void onViceContextResizeWindow(
        uint64_t window, int width, int height
    ) override;
    virtual void onViceContextFetchWindowImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> putImage
    ) override;
    virtual void onViceContextShutdownComplete() override;

    // WindowEventHandler:
    virtual void onWindowClose(uint64_t handle) override;
    virtual void onWindowCleanupComplete(uint64_t handle) override;
    virtual void onWindowViewImageChanged(uint64_t handle) override;

private:
    void afterConstruct_(shared_ptr<Server> self);

    void checkCleanupComplete_();

    weak_ptr<ServerEventHandler> eventHandler_;

    uint64_t nextWindowHandle_;
    enum {Running, WaitWindows, WaitViceContext, ShutdownComplete} state_;

    shared_ptr<ViceContext> viceCtx_;
    map<uint64_t, shared_ptr<Window>> openWindows_;
    map<uint64_t, shared_ptr<Window>> cleanupWindows_;
};

}
