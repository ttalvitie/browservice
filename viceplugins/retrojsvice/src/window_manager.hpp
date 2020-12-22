#include "window.hpp"

namespace retrojsvice {

class WindowManagerEventHandler {
public:
    // Exceptionally, onWindowManagerCreateWindowRequest and
    // onWindowManagerCloseWindow are called directly instead of the task queue
    // to ensure atomicity of window creation and destruction
    virtual variant<uint64_t, string> onWindowManagerCreateWindowRequest() = 0;
    virtual void onWindowManagerCloseWindow(uint64_t handle) = 0;

    // See ImageCompressorEventHandler::onImageCompressorFetchImage
    virtual void onWindowManagerFetchImage(
        uint64_t handle,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;
};

class HTTPRequest;

// Must be closed with close() prior to destruction
class WindowManager :
    public WindowEventHandler,
    public enable_shared_from_this<WindowManager>
{
SHARED_ONLY_CLASS(WindowManager);
public:
    WindowManager(CKey, shared_ptr<WindowManagerEventHandler> eventHandler);
    ~WindowManager();

    // Immediately closes all windows and prevents new windows from being
    // created; new HTTP requests are dropped immediately. Calls
    // WindowManagerEventHandler::onCloseWindow directly and drops shared
    // pointer to event handler.
    void close();

    void handleHTTPRequest(shared_ptr<HTTPRequest> request);

    shared_ptr<Window> tryGetWindow(uint64_t handle);

    // WindowEventHandler:
    virtual void onWindowClose(uint64_t handle) override;
    virtual void onWindowFetchImage(
        uint64_t handle,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) override;

private:
    void handleNewWindowRequest_(shared_ptr<HTTPRequest> request);

    shared_ptr<WindowManagerEventHandler> eventHandler_;
    bool closed_;

    map<uint64_t, shared_ptr<Window>> windows_;
};

}
