#include "task_queue.hpp"

namespace retrojsvice {

class WindowEventHandler {
public:
    // Exceptionally, onWindowClose is called directly instead of the task queue
    // to ensure atomicity of window destruction
    // When called, the window is closed immediately (after the call, no more
    // event handlers will be called and none of the member functions of Window
    // may be called; the Window also drops the shared pointer to the event
    // handler).
    virtual void onWindowClose(uint64_t handle) = 0;
};

// Must be closed before destruction (as signaled by the onWindowClose, caused
// by the Window itself or initiated using Window::close)
class Window {
SHARED_ONLY_CLASS(Window);
public:
    Window(CKey, shared_ptr<WindowEventHandler> eventHandler, uint64_t handle);
    ~Window();

    // Immediately closes the window, calling WindowEventHandler::onWindowClose
    // directly
    void close();

private:
    shared_ptr<WindowEventHandler> eventHandler_;
    uint64_t handle_;
    bool closed_;
};

}
