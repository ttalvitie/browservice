#include "window_manager.hpp"

#include "html.hpp"
#include "http.hpp"

namespace retrojsvice {

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

void WindowManager::close() {
    REQUIRE_API_THREAD();

    if(closed_) {
        return;
    }

    closed_ = true;
    eventHandler_.reset();
}

void WindowManager::handleHTTPRequest(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();

    if(closed_) {
        request->sendTextResponse(503, "ERROR: Service is shutting down\n");
        return;
    }

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        handleNewWindowRequest_(request);
        return;
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method\n");
}

void WindowManager::handleNewWindowRequest_(shared_ptr<HTTPRequest> request) {
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

            shared_ptr<Window> window = Window::create(handle);
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
