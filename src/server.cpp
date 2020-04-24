#include "server.hpp"

#include "globals.hpp"
#include "html.hpp"

namespace {

regex sessionPathRegex("/([0-9]+)/.*");

}

Server::Server(CKey, weak_ptr<ServerEventHandler> eventHandler) {
    CEF_REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    state_ = Running;
    // Setup is finished in afterConstruct_
}

void Server::shutdown() {
    CEF_REQUIRE_UI_THREAD();

    if(state_ == Running) {
        state_ = ShutdownPending;
        LOG(INFO) << "Shutting down server";

        httpServer_->shutdown();

        for(pair<uint64_t, shared_ptr<Session>> p : sessions_) {
            p.second->close();
        }
    }
}

void Server::onHTTPServerRequest(shared_ptr<HTTPRequest> request) {
    CEF_REQUIRE_UI_THREAD();

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        shared_ptr<Session> session = Session::create(shared_from_this());
        sessions_[session->id()] = session;

        request->sendHTMLResponse(200, writeNewSessionHTML, {session->id()});
        return;
    }

    smatch match;
    if(regex_match(path, match, sessionPathRegex)) {
        CHECK(match.size() == 2);
        optional<uint64_t> sessionID = parseString<uint64_t>(match[1]);
        if(sessionID) {
            auto it = sessions_.find(*sessionID);
            if(it != sessions_.end()) {
                it->second->handleHTTPRequest(request);
            } else {
                request->sendTextResponse(400, "ERROR: Invalid session ID");
            }
            return;
        }
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method");
}

void Server::onHTTPServerShutdownComplete() {
    CEF_REQUIRE_UI_THREAD();
    checkShutdownStatus_();
}

void Server::onSessionClosed(uint64_t id) {
    CEF_REQUIRE_UI_THREAD();

    auto it = sessions_.find(id);
    CHECK(it != sessions_.end());
    sessions_.erase(it);

    checkShutdownStatus_();
}

void Server::afterConstruct_(shared_ptr<Server> self) {
    httpServer_ = HTTPServer::create(self, globals->config->httpListenAddr);
}

void Server::checkShutdownStatus_() {
    if(
        state_ == ShutdownPending &&
        httpServer_->isShutdownComplete() &&
        sessions_.empty()
    ) {
        state_ = ShutdownComplete;
        LOG(INFO) << "Server shutdown complete";
        postTask(eventHandler_, &ServerEventHandler::onServerShutdownComplete);
    }
}
