#pragma once

#include "browser_area.hpp"
#include "control_bar.hpp"
#include "download_manager.hpp"
#include "image_slice.hpp"
#include "widget.hpp"

class CefBrowser;

namespace browservice {

class Window;

class WindowEventHandler {
public:
    // Called when window is closing for vice plugin (communication should
    // cease) but cannot be destructed yet as the CEF browser is still open.
    // Unless the window is closed with Window::close(), this is always called
    // before WindowEventHandler::onWindowClosed.
    virtual void onWindowClosing(uint64_t handle) = 0;

    // Called when window is completely closed and can be destructed.
    virtual void onWindowClosed(uint64_t handle) = 0;

    virtual void onWindowViewImageChanged(uint64_t handle) = 0;
};

class DownloadManager;
class RootWidget;
class Timeout;

// Single browser window. Before quitting CEF message loop, call close and wait
// for onWindowClosed event.
class Window :
    public WidgetParent,
    public ControlBarEventHandler,
    public BrowserAreaEventHandler,
    public DownloadManagerEventHandler,
    public enable_shared_from_this<Window>
{
SHARED_ONLY_CLASS(Window);
public:
    // Creates new window. The isPopup argument is only used internally to
    // create popup windows. Returns empty pointer if creating the window
    // failed
    static shared_ptr<Window> tryCreate(
        shared_ptr<WindowEventHandler> eventHandler,
        uint64_t handle,
        bool isPopup = false
    );

    // Private constructor
    Window(CKey, CKey);

    ~Window();

    // Close open window. WindowEventHandler::onWindowClosing will not be
    // called as the close is initiated by the user.
    void close();

    ImageSlice getViewImage();

    // WidgetParent:
    virtual void onWidgetViewDirty() override;
    virtual void onWidgetCursorChanged() override;
    virtual void onGlobalHotkeyPressed(GlobalHotkey key) override;

    // ControlBarEventHandler:
    virtual void onAddressSubmitted(string url) override;
    virtual void onQualityChanged(int quality) override;
    virtual void onPendingDownloadAccepted() override;
    virtual void onFind(string text, bool forward, bool findNext) override;
    virtual void onStopFind(bool clearSelection) override;
    virtual void onClipboardButtonPressed() override;

    // BrowserAreaEventHandler:
    virtual void onBrowserAreaViewDirty() override;

    // DownloadManagerEventHandler:
    virtual void onPendingDownloadCountChanged(int count) override;
    virtual void onDownloadProgressChanged(vector<int> progress) override;
    virtual void onDownloadCompleted(shared_ptr<CompletedDownload> file) override;

private:
    // Class that implements CefClient interfaces for this window
    class Client;

    void updateSecurityStatus_();

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

    // -1 = back, 0 = refresh, 1 = forward
    void navigate_(int direction);

    shared_ptr<WindowEventHandler> eventHandler_;

    uint64_t handle_;

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

    // Downloads whose iframe has been loaded, and the actual file is kept
    // available until a timeout has expired.
    map<uint64_t, pair<shared_ptr<CompletedDownload>, shared_ptr<Timeout>>> downloads_;
    uint64_t curDownloadIdx_;

    // The window consists of the vice plugin window (communicating through
    // event handlers and public member functions) and the CEF browser with
    // different lifetimes:
    //   - Pending: Vice plugin window open, CEF browser closed
    //   - Open: Both open
    //   - Closing: Vice plugin window closed, CEF browser open
    //   - Closed: Both closed
    enum {Pending, Open, Closing, Closed} state_;

    steady_clock::time_point lastNavigateOperationTime_;

    shared_ptr<Timeout> securityStatusUpdateTimeout_;

    ImageSlice rootViewport_;
    shared_ptr<RootWidget> rootWidget_;

    shared_ptr<DownloadManager> downloadManager_;

    // Only available in Open state
    CefRefPtr<CefBrowser> browser_;
};

}
