#include "server.hpp"

#include "globals.hpp"
#include "quality.hpp"
#include "xwindow.hpp"

namespace browservice {

Server::Server(CKey,
    weak_ptr<ServerEventHandler> eventHandler,
    shared_ptr<ViceContext> viceCtx
) {
    REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    state_ = Running;
    nextWindowHandle_ = 1;
    viceCtx_ = viceCtx;
    // Setup is finished in afterConstruct_
}

void Server::shutdown() {
    REQUIRE_UI_THREAD();

    if(state_ == Running) {
        state_ = WaitWindows;
        INFO_LOG("Shutting down server");

        map<uint64_t, shared_ptr<Window>> windows = windows_;
        for(pair<uint64_t, shared_ptr<Window>> p : windows) {
            p.second->close();
            viceCtx_->closeWindow(p.first);
        }

        checkWindowsEmpty_();
    }
}

uint64_t Server::onViceContextCreateWindowRequest(string& reason) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    if(state_ != Running) {
        reason = "Server is shutting down";
        return 0;
    }

    INFO_LOG("Got request for new window from vice plugin");

    if((int)windows_.size() >= globals->config->windowLimit) {
        INFO_LOG("Denying window creation due to window limit");
        reason = "Maximum number of concurrent windows exceeded";
        return 0;
    }

    uint64_t handle = nextWindowHandle_++;
    REQUIRE(handle);

    shared_ptr<Window> window = Window::tryCreate(shared_from_this(), handle);
    if(window) {
        windows_[handle] = window;
        return handle;
    } else {
        reason = "Creating browser for window failed";
        return 0;
    }
}

void Server::onViceContextCloseWindow(uint64_t window) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());

    it->second->close();
}

void Server::onViceContextFetchWindowImage(
    uint64_t window,
    function<void(const uint8_t*, size_t, size_t, size_t)> putImage
) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    auto it = windows_.find(window);
    REQUIRE(it != windows_.end());

    ImageSlice image = it->second->getViewImage();
    if(image.width() < 1 || image.height() < 1) {
        image = ImageSlice::createImage(1, 1);
    }
    putImage(image.buf(), image.width(), image.height(), image.pitch());
}

void Server::onViceContextShutdownComplete() {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == WaitViceContext);

    state_ = ShutdownComplete;
    INFO_LOG("Server shutdown complete");
    postTask(eventHandler_, &ServerEventHandler::onServerShutdownComplete);
}

void Server::onWindowClosing(uint64_t handle) {
    REQUIRE_UI_THREAD();
    PANIC("Window closing on its own; not implemented");
}

void Server::onWindowClosed(uint64_t handle) {
    REQUIRE_UI_THREAD();

    auto it = windows_.find(handle);
    REQUIRE(it != windows_.end());
    windows_.erase(it);

    checkWindowsEmpty_();
}

void Server::onWindowViewImageChanged(uint64_t handle) {
    REQUIRE_UI_THREAD();
    REQUIRE(windows_.count(handle));

    viceCtx_->notifyWindowViewChanged(handle);
}

void Server::afterConstruct_(shared_ptr<Server> self) {
    viceCtx_->start(self);
}

void Server::checkWindowsEmpty_() {
    if(state_ == WaitWindows && windows_.empty()) {
        state_ = WaitViceContext;
        viceCtx_->shutdown();
    }
}

}
