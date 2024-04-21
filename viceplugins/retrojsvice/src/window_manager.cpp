#include "window_manager.hpp"

#include "http.hpp"

namespace retrojsvice {

WindowManager::WindowManager(CKey,
    shared_ptr<WindowManagerEventHandler> eventHandler,
    shared_ptr<SecretGenerator> secretGen,
    string programName,
    int defaultQuality,
    bool setupNavigationForwarding
) {
    REQUIRE_API_THREAD();
    REQUIRE(defaultQuality >= 10 && defaultQuality <= 101);

    eventHandler_ = eventHandler;
    closed_ = false;

    secretGen_ = secretGen;
    programName_ = move(programName);
    defaultQuality_ = defaultQuality;
    setupNavigationForwarding_ = setupNavigationForwarding;
}

WindowManager::~WindowManager() {
    REQUIRE(closed_);
}

void WindowManager::close(MCE) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);
    REQUIRE(eventHandler_);

    closed_ = true;

    while(!windows_.empty()) {
        uint64_t handle = windows_.begin()->first;
        shared_ptr<Window> window = windows_.begin()->second;
        windows_.erase(handle);

        INFO_LOG("Closing window ", handle, " due to plugin shutdown");

        window->close();
        eventHandler_->onWindowManagerCloseWindow(handle);
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
        handleNewWindowRequest_(mce, request, {});
        return;
    }

    const string gotoPrefix = "/goto/";
    if(method == "GET" && path.size() >= gotoPrefix.size() && path.substr(0, gotoPrefix.size()) == gotoPrefix) {
        string uri = path.substr(gotoPrefix.size());
        handleNewWindowRequest_(mce, request, uri);
        return;
    }

    vector<string> pathSplit = splitStr(path, '/', 2);
    if(pathSplit.size() == 3 && pathSplit[0].empty() && isNonEmptyNumericStr(pathSplit[1])) {
        optional<uint64_t> handle = parseString<uint64_t>(pathSplit[1]);
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

bool WindowManager::createPopupWindow(
    uint64_t parentWindow,
    uint64_t popupWindow,
    string& reason
) {
    REQUIRE_API_THREAD();

    if(closed_) {
        reason = "Plugin is shutting down";
        return false;
    }

    auto it = windows_.find(parentWindow);
    REQUIRE(it != windows_.end());
    shared_ptr<Window> parentWindowPtr = it->second;

    REQUIRE(popupWindow);
    REQUIRE(!windows_.count(popupWindow));

    INFO_LOG(
        "Creating popup window ", popupWindow, " with parent ", parentWindow,
        " as requested by the program"
    );

    shared_ptr<Window> popupWindowPtr =
        parentWindowPtr->createPopup(popupWindow);
    REQUIRE(popupWindowPtr);
    REQUIRE(windows_.emplace(popupWindow, popupWindowPtr).second);

    return true;
}

void WindowManager::closeWindow(uint64_t window) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    shared_ptr<Window> windowPtr = it->second;
    windows_.erase(it);

    INFO_LOG("Closing window ", window, " as requested by program");

    windowPtr->close();
}

void WindowManager::notifyViewChanged(uint64_t window) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    it->second->notifyViewChanged();
}

void WindowManager::setCursor(uint64_t window, int cursorSignal) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    it->second->setCursor(cursorSignal);
}

optional<pair<vector<string>, size_t>> WindowManager::qualitySelectorQuery(
    uint64_t window
) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    return it->second->qualitySelectorQuery();
}

void WindowManager::qualityChanged(uint64_t window, size_t qualityIdx) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    return it->second->qualityChanged(qualityIdx);
}

bool WindowManager::needsClipboardButtonQuery(uint64_t window) {
    REQUIRE_API_THREAD();
    REQUIRE(windows_.count(window));
    return true;
}

void WindowManager::clipboardButtonPressed(uint64_t window) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    it->second->clipboardButtonPressed();
}

void WindowManager::putFileDownload(
    uint64_t window, shared_ptr<FileDownload> file
) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    it->second->putFileDownload(file);
}

bool WindowManager::startFileUpload(uint64_t window) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    return it->second->startFileUpload();
}

void WindowManager::cancelFileUpload(uint64_t window) {
    REQUIRE_API_THREAD();

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());
    it->second->cancelFileUpload();
}

void WindowManager::onWindowClose(uint64_t window) {
    REQUIRE_API_THREAD();
    REQUIRE(eventHandler_);

    REQUIRE(windows_.erase(window));
    eventHandler_->onWindowManagerCloseWindow(window);
}

#define FORWARD_WINDOW_EVENT(src, dest) \
    void WindowManager::src { \
        REQUIRE_API_THREAD(); \
        REQUIRE(!closed_); \
        REQUIRE(eventHandler_); \
        REQUIRE(windows_.count(window)); \
        eventHandler_->dest; \
    }

FORWARD_WINDOW_EVENT(
    onWindowFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ),
    onWindowManagerFetchImage(window, func)
)
FORWARD_WINDOW_EVENT(
    onWindowResize(uint64_t window, size_t width, size_t height),
    onWindowManagerResizeWindow(window, width, height)
)
FORWARD_WINDOW_EVENT(
    onWindowMouseDown(uint64_t window, int x, int y, int button),
    onWindowManagerMouseDown(window, x, y, button)
)
FORWARD_WINDOW_EVENT(
    onWindowMouseUp(uint64_t window, int x, int y, int button),
    onWindowManagerMouseUp(window, x, y, button)
)
FORWARD_WINDOW_EVENT(
    onWindowMouseMove(uint64_t window, int x, int y),
    onWindowManagerMouseMove(window, x, y)
)
FORWARD_WINDOW_EVENT(
    onWindowMouseDoubleClick(uint64_t window, int x, int y, int button),
    onWindowManagerMouseDoubleClick(window, x, y, button)
)
FORWARD_WINDOW_EVENT(
    onWindowMouseWheel(uint64_t window, int x, int y, int delta),
    onWindowManagerMouseWheel(window, x, y, delta)
)
FORWARD_WINDOW_EVENT(
    onWindowMouseLeave(uint64_t window, int x, int y),
    onWindowManagerMouseLeave(window, x, y)
)
FORWARD_WINDOW_EVENT(
    onWindowKeyDown(uint64_t window, int key),
    onWindowManagerKeyDown(window, key)
)
FORWARD_WINDOW_EVENT(
    onWindowKeyUp(uint64_t window, int key),
    onWindowManagerKeyUp(window, key)
)
FORWARD_WINDOW_EVENT(
    onWindowLoseFocus(uint64_t window),
    onWindowManagerLoseFocus(window)
)
FORWARD_WINDOW_EVENT(
    onWindowNavigate(uint64_t window, int direction),
    onWindowManagerNavigate(window, direction)
)
FORWARD_WINDOW_EVENT(
    onWindowNavigateToURI(uint64_t window, string uri),
    onWindowManagerNavigateToURI(window, move(uri))
)
FORWARD_WINDOW_EVENT(
    onWindowUploadFile(
        uint64_t window, string name, shared_ptr<FileUpload> file
    ),
    onWindowManagerUploadFile(window, move(name), file)
)
FORWARD_WINDOW_EVENT(
    onWindowCancelFileUpload(uint64_t window),
    onWindowManagerCancelFileUpload(window)
)

namespace {

bool hasPNGSupport(string userAgent) {
    for(char& c : userAgent) {
        c = tolower(c);
    }
    return
        userAgent.find("windows 3.1") == string::npos &&
        userAgent.find("win16") == string::npos &&
        userAgent.find("windows 16-bit") == string::npos;
}

}

void WindowManager::handleNewWindowRequest_(MCE, shared_ptr<HTTPRequest> request, optional<string> uri) {
    REQUIRE(!closed_);
    REQUIRE(eventHandler_);

    INFO_LOG("New window requested by user");

    variant<uint64_t, string> result;
    if(uri.has_value()) {
        result = eventHandler_->onWindowManagerCreateWindowWithURIRequest(uri.value());
    } else {
        result = eventHandler_->onWindowManagerCreateWindowRequest();
    }

    visit(Overloaded {
        [&](uint64_t handle) {
            INFO_LOG("Creating window ", handle);

            REQUIRE(handle);
            REQUIRE(!windows_.count(handle));

            bool allowPNG = hasPNGSupport(request->userAgent());
            shared_ptr<Window> window = Window::create(
                shared_from_this(),
                handle,
                secretGen_,
                programName_,
                allowPNG,
                defaultQuality_,
                setupNavigationForwarding_
            );
            REQUIRE(windows_.emplace(handle, window).second);

            window->handleInitialForwardHTTPRequest(request);
        },
        [&](string msg) {
            INFO_LOG("Window creation denied (reason: ", msg, ")");

            request->sendTextResponse(
                503, "ERROR: Could not create window, reason: " + msg + "\n"
            );
        }
    }, result);
}

}
