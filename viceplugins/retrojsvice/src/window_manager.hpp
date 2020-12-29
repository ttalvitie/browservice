#include "window.hpp"

namespace retrojsvice {

class WindowManagerEventHandler {
public:
    virtual variant<uint64_t, string> onWindowManagerCreateWindowRequest() = 0;
    virtual void onWindowManagerCloseWindow(uint64_t window) = 0;

    virtual void onWindowManagerResizeWindow(
        uint64_t window,
        size_t width,
        size_t height
    ) = 0;

    // See ImageCompressorEventHandler::onImageCompressorFetchImage
    virtual void onWindowManagerFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;

    virtual void onWindowManagerMouseDown(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onWindowManagerMouseUp(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onWindowManagerMouseMove(uint64_t window, int x, int y) = 0;
    virtual void onWindowManagerMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onWindowManagerMouseWheel(
        uint64_t window, int x, int y, int delta
    ) = 0;
    virtual void onWindowManagerMouseLeave(uint64_t window, int x, int y) = 0;

    virtual void onWindowManagerKeyDown(uint64_t window, int key) = 0;
    virtual void onWindowManagerKeyUp(uint64_t window, int key) = 0;

    virtual void onWindowManagerLoseFocus(uint64_t window) = 0;
};

class HTTPRequest;

// Must be closed with close() prior to destruction.
class WindowManager :
    public WindowEventHandler,
    public enable_shared_from_this<WindowManager>
{
SHARED_ONLY_CLASS(WindowManager);
public:
    WindowManager(CKey, shared_ptr<WindowManagerEventHandler> eventHandler);
    ~WindowManager();

    // Immediately closes all windows and prevents new windows from being
    // created; new HTTP requests are dropped immediately. May call
    // WindowManagerEventHandler::onCloseWindow directly and drops shared
    // pointer to event handler. Will not call any other event handlers.
    void close(MCE);

    void handleHTTPRequest(MCE, shared_ptr<HTTPRequest> request);

    void notifyViewChanged(uint64_t window);

    // WindowEventHandler:
    virtual void onWindowClose(uint64_t window) override;
    virtual void onWindowResize(
        uint64_t window,
        size_t width,
        size_t height
    ) override;
    virtual void onWindowFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) override;
    virtual void onWindowMouseDown(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowMouseUp(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowMouseMove(uint64_t window, int x, int y) override;
    virtual void onWindowMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowMouseWheel(
        uint64_t window, int x, int y, int delta
    ) override;
    virtual void onWindowMouseLeave(uint64_t window, int x, int y) override;
    virtual void onWindowKeyDown(uint64_t window, int key) override;
    virtual void onWindowKeyUp(uint64_t window, int key) override;
    virtual void onWindowLoseFocus(uint64_t window) override;

private:
    void handleNewWindowRequest_(MCE, shared_ptr<HTTPRequest> request);

    shared_ptr<WindowManagerEventHandler> eventHandler_;
    bool closed_;

    map<uint64_t, shared_ptr<Window>> windows_;
};

}
