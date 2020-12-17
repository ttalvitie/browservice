#include "window.hpp"

namespace retrojsvice {

Window::Window(CKey, shared_ptr<WindowEventHandler> eventHandler, uint64_t handle) {
    REQUIRE_API_THREAD();
    REQUIRE(handle);

    eventHandler_ = eventHandler;
    handle_ = handle;
    closed_ = false;
}

Window::~Window() {
    REQUIRE(closed_);
}

void Window::close() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    closed_ = true;

    REQUIRE(eventHandler_);
    eventHandler_->onWindowClose(handle_);

    eventHandler_.reset();
}

}
