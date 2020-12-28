#include "window.hpp"

namespace retrojsvice {

class WindowManagerEventHandler {
public:
    virtual variant<uint64_t, string> onWindowManagerCreateWindowRequest() = 0;
    virtual void onWindowManagerCloseWindow(uint64_t handle) = 0;

    // See ImageCompressorEventHandler::onImageCompressorFetchImage
    virtual void onWindowManagerFetchImage(
        uint64_t handle,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;
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

    void notifyViewChanged(uint64_t handle);

    // WindowEventHandler:
    virtual void onWindowClose(uint64_t handle) override;
    virtual void onWindowFetchImage(
        uint64_t handle,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) override;

private:
    void handleNewWindowRequest_(MCE, shared_ptr<HTTPRequest> request);

    shared_ptr<WindowManagerEventHandler> eventHandler_;
    bool closed_;

    map<uint64_t, shared_ptr<Window>> windows_;
};

}
