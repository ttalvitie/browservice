#include "server.hpp"

#include "globals.hpp"
#include "html.hpp"

namespace {

regex sessionPathRegex("/([0-9]+)/.*");

}

Server::Server(CKey, weak_ptr<ServerEventHandler> eventHandler) {
    CEF_REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
}

void Server::shutdown() {
    CEF_REQUIRE_UI_THREAD();
    httpServer_->shutdown();
}

void Server::onHTTPServerRequest(shared_ptr<HTTPRequest> request) {
    CEF_REQUIRE_UI_THREAD();

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        shared_ptr<Session> session = Session::create();
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
    CHECK(httpServer_->isShutdownComplete());
    postTask(eventHandler_, &ServerEventHandler::onServerShutdownComplete);
}

void Server::afterConstruct_(shared_ptr<Server> self) {
    CEF_REQUIRE_UI_THREAD();
    httpServer_ = HTTPServer::create(self, globals->config->httpListenAddr);
}
