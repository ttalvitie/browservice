#pragma once

#include "http.hpp"

class SessionEventHandler {
public:
    virtual void onSessionClosed(uint64_t id) = 0;
};

class CefBrowser;
class Timeout;

// Single browser session. Before quitting CEF message loop, call close and wait
// for onSessionClosed event. 
class Session : public enable_shared_from_this<Session> {
SHARED_ONLY_CLASS(Session);
public:
    Session(CKey, weak_ptr<SessionEventHandler> eventHandler);
    ~Session();

    // Close browser if it is not yet closed
    void close();

    void handleHTTPRequest(shared_ptr<HTTPRequest> request);

    // Get the unique and constant ID of this session
    uint64_t id();

private:
    // Class that implements CefClient interfaces for this session
    class Client;

    void afterConstruct_(shared_ptr<Session> self);

    void updateInactivityTimeout_();

    weak_ptr<SessionEventHandler> eventHandler_;

    uint64_t id_;

    bool prePrevVisited_;
    bool preMainVisited_;

    enum {Pending, Open, Closing, Closed} state_;

    // If true, browser should close as soon as it is opened
    bool closeOnOpen_;

    shared_ptr<Timeout> inactivityTimeout_;

    // Only available in Open state
    CefRefPtr<CefBrowser> browser_;
};
