#pragma once

#include "http.hpp"

class ServerEventHandler {
public:
    virtual void onServerShutdownComplete() = 0;
};

class Server : public HTTPServerEventHandler {
SHARED_ONLY_CLASS(Server);
public:
    Server(CKey, weak_ptr<ServerEventHandler> eventHandler);

    void shutdown();

    // HTTPServerEventHandler:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request);
    virtual void onHTTPServerShutdownComplete() override;

private:
    void afterConstruct_(shared_ptr<Server> self);

    weak_ptr<ServerEventHandler> eventHandler_;
    shared_ptr<HTTPServer> httpServer_;
};
