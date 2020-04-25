#pragma once

#include "http.hpp"
#include "image_slice.hpp"
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

    // Change root viewport size if it is different than currently. Clamps
    // dimensions to sane interval.
    void updateRootViewportSize_(int width, int height);

    weak_ptr<SessionEventHandler> eventHandler_;

    uint64_t id_;

    bool prePrevVisited_;
    bool preMainVisited_;

    // How many times the main page has been requested. The main page mentions
    // its index to all the requests it makes, and we discard all the requests
    // that are not from the newest main page.
    uint64_t curMainIdx_;

    // Latest image index. We discard image requests that do not have a higher
    // image index to avoid request reordering.
    uint64_t curImgIdx_;

    enum {Pending, Open, Closing, Closed} state_;

    // If true, browser should close as soon as it is opened
    bool closeOnOpen_;

    shared_ptr<Timeout> inactivityTimeout_;

    shared_ptr<ImageCompressor> imageCompressor_;

    ImageSlice rootViewport_;
    shared_ptr<RootWidget> rootWidget_;

    // Only available in Open state
    CefRefPtr<CefBrowser> browser_;
};
