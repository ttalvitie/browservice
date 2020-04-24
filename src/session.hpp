#pragma once

#include "http.hpp"
#include "widget.hpp"

class SessionEventHandler {
public:
    virtual void onSessionClosed(uint64_t id) = 0;
};

class ImageCompressor;
class RootWidget;
class Timeout;

class CefBrowser;

// Single browser session. Before quitting CEF message loop, call close and wait
// for onSessionClosed event. 
class Session :
    public WidgetEventHandler,
    public enable_shared_from_this<Session> {
SHARED_ONLY_CLASS(Session);
public:
    Session(CKey, weak_ptr<SessionEventHandler> eventHandler);
    ~Session();

    // Close browser if it is not yet closed
    void close();

    void handleHTTPRequest(shared_ptr<HTTPRequest> request);

    // Get the unique and constant ID of this session
    uint64_t id();

    // WidgetEventHandler:
    virtual void onWidgetViewDirty() override;

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

    shared_ptr<ImageCompressor> imageCompressor_;

    shared_ptr<Widget> rootWidget_;

    // Only available in Open state
    CefRefPtr<CefBrowser> browser_;
};
