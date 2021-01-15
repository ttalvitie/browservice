#pragma once

#include "browser_area.hpp"
#include "control_bar.hpp"
#include "image_slice.hpp"
#include "root_widget.hpp"

class CefBrowser;

namespace browservice {

class WindowEventHandler {
public:
    virtual void onWindowClose(uint64_t handle) = 0;
    virtual void onWindowCleanupComplete(uint64_t handle) = 0;
    virtual void onWindowViewImageChanged(uint64_t handle) = 0;
};

class Timeout;

// Lifecycle of a Window:
//
// 1. Window becomes open immediately after being created using tryCreate;
//    an open window can be interacted with using the public member functions
//    and it may call event handlers. (It may take time for the actual browser
//    to start, but the UI is started immediately.)
//
// 2. Window is closed (instantiated either externally using the close() member
//    function or internally, signaled by a call to the onWindowClose event
//    handler). After this, the window is closed immediately, and all
//    interaction through the public member functions and event handlers (except
//    for the onWindowCleanupComplete event handler) must cease. However, the
//    window still exists and is in cleanup mode.
//
// 3. As the cleanup is complete (i.e. the CEF browser has been successfully
//    closed), the onWindowCleanupComplete event handler is called. After this, 
//    the Window drops the pointer to the event handler and the window object
//    may be destructed
class Window :
    public WidgetParent,
    public ControlBarEventHandler,
    public BrowserAreaEventHandler,
    public enable_shared_from_this<Window>
{
SHARED_ONLY_CLASS(Window);
public:
    // Returns empty pointer if CEF browser creation fails.
    static shared_ptr<Window> tryCreate(
        shared_ptr<WindowEventHandler> eventHandler,
        uint64_t handle
    );

    // Private constructor.
    Window(CKey, CKey);

    // Before destruction, ensure that the cleanup is complete.
    ~Window();

    void close();
    void resize(int width, int height);
    ImageSlice getViewImage();

    // Functions for passing input events to the Window. All arguments are be
    // sanitized.
    void sendMouseDownEvent(int x, int y, int button);
    void sendMouseUpEvent(int x, int y, int button);
    void sendMouseMoveEvent(int x, int y);
    void sendMouseDoubleClickEvent(int x, int y, int button);
    void sendMouseWheelEvent(int x, int y, int dx, int dy);
    void sendMouseLeaveEvent(int x, int y);
    void sendKeyDownEvent(int key);
    void sendKeyUpEvent(int key);
    void sendLoseFocusEvent();

    // WidgetParent:
    virtual void onWidgetViewDirty() override;
    virtual void onWidgetCursorChanged() override {}
    virtual void onGlobalHotkeyPressed(GlobalHotkey key) override {}

    // ControlBarEventHandler:
    virtual void onAddressSubmitted(string url) override {}
    virtual void onQualityChanged(int quality) override {}
    virtual void onPendingDownloadAccepted() override {}
    virtual void onFind(string text, bool forward, bool findNext) override {}
    virtual void onStopFind(bool clearSelection) override {}
    virtual void onClipboardButtonPressed() override {}

    // BrowserAreaEventHandler:
    virtual void onBrowserAreaViewDirty() override;

private:
    // Class that implements CefClient interfaces for the window.
    class Client;

    void watchdog_();
    void updateSecurityStatus_();

    void clampMouseCoords_(int& x, int& y);

    uint64_t handle_;
    enum {Open, Closed, CleanupComplete} state_;

    // Empty only in CleanupComplete state.
    shared_ptr<WindowEventHandler> eventHandler_;

    // Always empty in CleanupComplete state. May be empty in Open and Closed
    // states if the browser has not yet started.
    CefRefPtr<CefBrowser> browser_;

    ImageSlice rootViewport_;
    shared_ptr<RootWidget> rootWidget_;

    shared_ptr<Timeout> watchdogTimeout_;
};

}
