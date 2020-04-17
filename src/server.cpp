#include "server.hpp"

Server::Server(CKey, weak_ptr<ServerEventHandler> eventHandler) {
    eventHandler_ = eventHandler;
}

void Server::shutdown() {
    postTask(eventHandler_, &ServerEventHandler::onServerShutdownComplete);
}
