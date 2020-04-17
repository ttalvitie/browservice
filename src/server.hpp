#include "common.hpp"

class ServerEventHandler {
public:
    virtual void onServerShutdownComplete() = 0;
};

class Server {
SHARED_ONLY_CLASS(Server);
public:
    Server(CKey, weak_ptr<ServerEventHandler> eventHandler);

    void shutdown();

private:
    weak_ptr<ServerEventHandler> eventHandler_;
};
