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
    virtual uint64_t onViceContextCreateWindowRequest(string& reason, optional<string> uri) override;
    virtual void onViceContextCloseWindow(uint64_t window) override;
    virtual void onViceContextResizeWindow(
        uint64_t window, int width, int height
    ) override;
    virtual void onViceContextFetchWindowImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> putImage
    ) override;
    virtual void onViceContextMouseDown(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onViceContextMouseUp(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onViceContextMouseMove(uint64_t window, int x, int y) override;
    virtual void onViceContextMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onViceContextMouseWheel(
        uint64_t window, int x, int y, int dx, int dy
    ) override;
    virtual void onViceContextMouseLeave(uint64_t window, int x, int y) override;
    virtual void onViceContextKeyDown(uint64_t window, int key) override;
    virtual void onViceContextKeyUp(uint64_t window, int key) override;
    virtual void onViceContextLoseFocus(uint64_t window) override;
    virtual void onViceContextNavigate(uint64_t window, int direction) override;
    virtual void onViceContextNavigateToURI(uint64_t window, string uri) override;
    virtual void onViceContextCopyToClipboard(string text) override;
    virtual void onViceContextRequestClipboardContent() override;
    virtual void onViceContextUploadFile(
        uint64_t window, shared_ptr<ViceFileUpload> file
    ) override;
    virtual void onViceContextCancelFileUpload(uint64_t window) override;
    virtual string onViceContextWindowTitleQuery(uint64_t window) override;
    virtual void onViceContextShutdownComplete() override;

    // WindowEventHandler:
    virtual void onWindowClose(uint64_t handle) override;
    virtual void onWindowCleanupComplete(uint64_t handle) override;
    virtual void onWindowViewImageChanged(uint64_t handle) override;
    virtual void onWindowTitleChanged(uint64_t handle) override;
    virtual void onWindowCursorChanged(uint64_t handle, int cursor) override;
    virtual optional<pair<vector<string>, size_t>> onWindowQualitySelectorQuery(
        uint64_t handle
    ) override;
    virtual void onWindowQualityChanged(uint64_t handle, size_t idx) override;
    virtual bool onWindowNeedsClipboardButtonQuery(uint64_t handle) override;
    virtual void onWindowClipboardButtonPressed(uint64_t handle) override;
    virtual void onWindowDownloadCompleted(
        uint64_t handle, shared_ptr<CompletedDownload> file
    ) override;
    virtual bool onWindowStartFileUpload(uint64_t handle) override;
    virtual void onWindowCancelFileUpload(uint64_t handle) override;
    virtual void onWindowCreatePopupRequest(
        uint64_t handle,
        function<shared_ptr<Window>(uint64_t)> accept
    ) override;

private:
    void afterConstruct_(shared_ptr<Server> self);

    void checkCleanupComplete_();

    weak_ptr<ServerEventHandler> eventHandler_;

    uint64_t nextWindowHandle_;
    enum {Running, WaitWindows, WaitViceContext, ShutdownComplete} state_;

    shared_ptr<ViceContext> viceCtx_;
    map<uint64_t, shared_ptr<Window>> openWindows_;
    map<uint64_t, shared_ptr<Window>> cleanupWindows_;

    bool clipboardContentRequested_;
};

}
