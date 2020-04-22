#pragma once

#include "http.hpp"

class Session {
SHARED_ONLY_CLASS(Session);
public:
    Session(CKey);
    ~Session();

    void handleHTTPRequest(shared_ptr<HTTPRequest> request);

    // Get the unique and constant ID of this session
    uint64_t id();

private:
    uint64_t id_;
    bool prePrevVisited_;
    bool preMainVisited_;
};
