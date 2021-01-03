#include "server.hpp"

#include "globals.hpp"
#include "quality.hpp"
#include "xwindow.hpp"

namespace {

regex sessionPathRegex("/([0-9]+)/.*");

}

Server::Server(CKey,
    weak_ptr<ServerEventHandler> eventHandler,
    shared_ptr<ViceContext> viceCtx
) {
    REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    state_ = Running;
    nextSessionID_ = 1;
    viceCtx_ = viceCtx;
    // Setup is finished in afterConstruct_
}

void Server::shutdown() {
    REQUIRE_UI_THREAD();

    if(state_ == Running) {
        state_ = WaitSessions;
        INFO_LOG("Shutting down server");

        map<uint64_t, shared_ptr<Session>> sessions = sessions_;
        for(pair<uint64_t, shared_ptr<Session>> p : sessions) {
            p.second->close();
            viceCtx_->closeWindow(p.first);
        }

        checkSessionsEmpty_();
    }
}

uint64_t Server::onViceContextCreateWindowRequest(string& reason) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    if(state_ != Running) {
        reason = "Server is shutting down";
        return 0;
    }

    INFO_LOG("Got request for new session from vice plugin");

    if((int)sessions_.size() >= globals->config->sessionLimit) {
        INFO_LOG("Denying session creation due to session limit");
        reason = "Maximum number of concurrent sessions exceeded";
        return 0;
    }

    uint64_t id = nextSessionID_++;
    REQUIRE(id);

    shared_ptr<Session> session = Session::tryCreate(shared_from_this(), id);
    if(session) {
        sessions_[id] = session;
        return id;
    } else {
        reason = "Creating browser for session failed";
        return 0;
    }
}

void Server::onViceContextCloseWindow(uint64_t window) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    auto it = sessions_.find(window);
    REQUIRE(it != sessions_.end());

    it->second->close();
}

void Server::onViceContextFetchWindowImage(
    uint64_t window,
    function<void(const uint8_t*, size_t, size_t, size_t)> putImage
) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    auto it = sessions_.find(window);
    REQUIRE(it != sessions_.end());

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

void Server::onSessionClosing(uint64_t id) {
    REQUIRE_UI_THREAD();
    PANIC("Session closing on its own; not implemented");
}

void Server::onSessionClosed(uint64_t id) {
    REQUIRE_UI_THREAD();

    auto it = sessions_.find(id);
    REQUIRE(it != sessions_.end());
    sessions_.erase(it);

    checkSessionsEmpty_();
}

void Server::onSessionViewImageChanged(uint64_t id) {
    REQUIRE_UI_THREAD();
    REQUIRE(sessions_.count(id));

    viceCtx_->notifyWindowViewChanged(id);
}

void Server::afterConstruct_(shared_ptr<Server> self) {
    viceCtx_->start(self);
}

void Server::checkSessionsEmpty_() {
    if(state_ == WaitSessions && sessions_.empty()) {
        state_ = WaitViceContext;
        viceCtx_->shutdown();
    }
}
