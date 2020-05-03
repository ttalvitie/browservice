#pragma once

#include "browser_area.hpp"
#include "http.hpp"
#include "image_slice.hpp"
#include "widget.hpp"

class Session;

class SessionEventHandler {
public:
    virtual void onSessionClosed(uint64_t id) = 0;

    // Exceptionally, onPopupSessionOpen is called directly instead of the
    // event loop to avoid race conditions.
    virtual void onPopupSessionOpen(shared_ptr<Session> session) = 0;
};

class ImageCompressor;
class RootWidget;
class Timeout;

class CefBrowser;

// Single browser session. Before quitting CEF message loop, call close and wait
// for onSessionClosed event. 
class Session :
    public WidgetEventHandler,
    public BrowserAreaEventHandler,
    public enable_shared_from_this<Session>
{
SHARED_ONLY_CLASS(Session);
public:
    // Creates new session. The isPopup argument is only used internally to
    // create popup sessions.
    Session(CKey,
        weak_ptr<SessionEventHandler> eventHandler,
        bool isPopup = false
    );

    ~Session();

    // Close browser if it is not yet closed
    void close();

    void handleHTTPRequest(shared_ptr<HTTPRequest> request);

    // Get the unique and constant ID of this session
    uint64_t id();

    // WidgetEventHandler:
    virtual void onWidgetViewDirty() override;

    // BrowserAreaEventHandler:
    virtual void onBrowserAreaViewDirty() override;

private:
    // Class that implements CefClient interfaces for this session
    class Client;

    void afterConstruct_(shared_ptr<Session> self);

    void updateInactivityTimeout_();

    // Change root viewport size if it is different than currently. Clamps
    // dimensions to sane interval.
    void updateRootViewportSize_(int width, int height);

    // Send paddedRootViewport_ to the image compressor correctly clamped such
    // that its dimensions result in signals (widthSignal_, heightSignal_).
    void sendViewportToCompressor_();

    void handleEvents_(
        uint64_t startIdx,
        string::const_iterator begin,
        string::const_iterator end
    );

    void setWidthSignal_(int newWidthSignal);
    void setHeightSignal_(int newHeightSignal);

    weak_ptr<SessionEventHandler> eventHandler_;

    uint64_t id_;

    bool isPopup_;

    bool prePrevVisited_;
    bool preMainVisited_;

    bool prevNextClicked_;

    // How many times the main page has been requested. The main page mentions
    // its index to all the requests it makes, and we discard all the requests
    // that are not from the newest main page.
    uint64_t curMainIdx_;

    // Latest image index. We discard image requests that do not have a higher
    // image index to avoid request reordering.
    uint64_t curImgIdx_;

    // How many events we have handled for the current main index. We keep track
    // of this to avoid replaying events; the client may send the same events
    // twice as it cannot know for sure which requests make it through.
    uint64_t curEventIdx_;

    enum {Pending, Open, Closing, Closed} state_;

    // If true, browser should close as soon as it is opened
    bool closeOnOpen_;

    shared_ptr<Timeout> inactivityTimeout_;

    shared_ptr<ImageCompressor> imageCompressor_;

    ImageSlice paddedRootViewport_;
    ImageSlice rootViewport_;
    shared_ptr<RootWidget> rootWidget_;

    // We use width and height modulo WidthSignalModulus and HeightSignalModulus
    // of the part of rootViewport_ sent to imageCompressor_ to signal various
    // things to the client. We make the initial signals (1, 1) as
    // imageCompressor_ initially sends a 1x1 image.
    static constexpr int WidthSignalNewIframe = 0;
    static constexpr int WidthSignalNoNewIframe = 1;
    static constexpr int WidthSignalModulus = 2;

    static constexpr int HeightSignalHandCursor = 0;
    static constexpr int HeightSignalNormalCursor = 1;
    static constexpr int HeightSignalIBeamCursor = 2;
    static constexpr int HeightSignalModulus = 3;

    int widthSignal_;
    int heightSignal_;

    // Only available in Open state
    CefRefPtr<CefBrowser> browser_;
};
