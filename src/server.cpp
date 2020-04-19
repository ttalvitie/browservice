#include "server.hpp"

#include "globals.hpp"

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
    LOG(INFO) << request->method() << " " << request->url();
    request->sendTextResponse(200, "TEST");
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
