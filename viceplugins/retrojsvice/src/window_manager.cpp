#include "window_manager.hpp"

#include "html.hpp"
#include "http.hpp"

namespace retrojsvice {

namespace {

regex windowPathRegex("/([0-9]+)/.*");

}

WindowManager::WindowManager(CKey,
    shared_ptr<WindowManagerEventHandler> eventHandler
) {
    REQUIRE_API_THREAD();

    eventHandler_ = eventHandler;
    closed_ = false;
}

WindowManager::~WindowManager() {
    REQUIRE(closed_);
}

void WindowManager::close(MCE) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    closed_ = true;

    while(!windows_.empty()) {
        uint64_t handle = windows_.begin()->first;
        shared_ptr<Window> window = windows_.begin()->second;

        INFO_LOG("Closing window ", handle, " due to plugin shutdown");

        window->close(mce);
        REQUIRE(!windows_.count(handle));
    }

    eventHandler_.reset();
}

void WindowManager::handleHTTPRequest(MCE, shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();

    if(closed_) {
        request->sendTextResponse(503, "ERROR: Service is shutting down\n");
        return;
    }

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        handleNewWindowRequest_(mce, request);
        return;
    }

    smatch match;
    if(regex_match(path, match, windowPathRegex)) {
        REQUIRE(match.size() == 2);
        optional<uint64_t> handle = parseString<uint64_t>(match[1]);
        if(handle) {
            auto it = windows_.find(*handle);
            if(it != windows_.end()) {
                shared_ptr<Window> window = it->second;
                window->handleHTTPRequest(mce, request);
            } else {
                request->sendTextResponse(400, "ERROR: Invalid window handle\n");
            }
            return;
        }
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method\n");
}

void WindowManager::notifyViewChanged(uint64_t handle) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(handle);
    REQUIRE(it != windows_.end());
    it->second->notifyViewChanged();
}

void WindowManager::onWindowClose(uint64_t handle) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);

    REQUIRE(windows_.erase(handle));
    eventHandler_->onWindowManagerCloseWindow(handle);
}

void WindowManager::onWindowResize(
    uint64_t handle,
    size_t width,
    size_t height
) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerResizeWindow(handle, width, height);
}

void WindowManager::onWindowFetchImage(
    uint64_t handle,
    function<void(const uint8_t*, size_t, size_t, size_t)> func
) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerFetchImage(handle, func);
}

void WindowManager::onWindowMouseDown(
    uint64_t handle, int x, int y, int button
) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerMouseDown(handle, x, y, button);
}

void WindowManager::onWindowMouseUp(uint64_t handle, int x, int y, int button) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerMouseUp(handle, x, y, button);
}

void WindowManager::onWindowMouseMove(uint64_t handle, int x, int y) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerMouseMove(handle, x, y);
}

void WindowManager::onWindowMouseDoubleClick(
    uint64_t handle, int x, int y, int button
) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerMouseDoubleClick(handle, x, y, button);
}

void WindowManager::onWindowMouseWheel(
    uint64_t handle, int x, int y, int delta
) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerMouseWheel(handle, x, y, delta);
}

void WindowManager::onWindowMouseLeave(uint64_t handle, int x, int y) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerMouseLeave(handle, x, y);
}

void WindowManager::onWindowLoseFocus(uint64_t handle) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);
    REQUIRE(windows_.count(handle));

    eventHandler_->onWindowManagerLoseFocus(handle);
}

void WindowManager::handleNewWindowRequest_(MCE, shared_ptr<HTTPRequest> request) {
    REQUIRE(!closed_);
    REQUIRE(eventHandler_);

    INFO_LOG("New window requested by user");

    variant<uint64_t, string> result =
        eventHandler_->onWindowManagerCreateWindowRequest();

    visit(Overloaded {
        [&](uint64_t handle) {
            INFO_LOG("Creating window ", handle);

            REQUIRE(handle);
            REQUIRE(!windows_.count(handle));

            shared_ptr<Window> window =
                Window::create(shared_from_this(), handle);
            REQUIRE(windows_.emplace(handle, window).second);

            request->sendHTMLResponse(200, writeNewWindowHTML, {handle});
        },
        [&](string msg) {
            INFO_LOG("Window creation denied by program (reason: ", msg, ")");

            request->sendTextResponse(
                503, "ERROR: Could not create window, reason: " + msg + "\n"
            );
        }
    }, result);
}

}
