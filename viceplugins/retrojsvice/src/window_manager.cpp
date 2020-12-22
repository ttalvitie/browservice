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

void WindowManager::close() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    closed_ = true;

    while(!windows_.empty()) {
        uint64_t handle = windows_.begin()->first;
        shared_ptr<Window> window = windows_.begin()->second;

        INFO_LOG("Closing window ", handle, " due to plugin shutdown");

        window->close();
        REQUIRE(!windows_.count(handle));
    }

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

    smatch match;
    if(regex_match(path, match, windowPathRegex)) {
        REQUIRE(match.size() == 2);
        optional<uint64_t> handle = parseString<uint64_t>(match[1]);
        if(handle) {
            auto it = windows_.find(*handle);
            if(it != windows_.end()) {
                shared_ptr<Window> window = it->second;
                window->handleHTTPRequest(request);
            } else {
                request->sendTextResponse(400, "ERROR: Invalid window handle\n");
            }
            return;
        }
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method\n");
}

shared_ptr<Window> WindowManager::tryGetWindow(uint64_t handle) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(handle);
    if(it == windows_.end()) {
        return shared_ptr<Window>();
    } else {
        return it->second;
    }
}

void WindowManager::onWindowClose(uint64_t handle) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);

    REQUIRE(windows_.erase(handle));
    eventHandler_->onWindowManagerCloseWindow(handle);
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
