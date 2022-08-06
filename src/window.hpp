#pragma once

#include "browser_area.hpp"
#include "control_bar.hpp"
#include "download_manager.hpp"
#include "image_slice.hpp"
#include "root_widget.hpp"

class CefBrowser;
class CefFileDialogCallback;

namespace browservice {

class Window;

class WindowEventHandler {
public:
    virtual void onWindowClose(uint64_t handle) = 0;
    virtual void onWindowCleanupComplete(uint64_t handle) = 0;
    virtual void onWindowViewImageChanged(uint64_t handle) = 0;
    virtual void onWindowCursorChanged(uint64_t handle, int cursor) = 0;
    virtual optional<pair<vector<string>, size_t>> onWindowQualitySelectorQuery(
        uint64_t handle
    ) = 0;
    virtual void onWindowQualityChanged(uint64_t handle, size_t idx) = 0;
    virtual bool onWindowNeedsClipboardButtonQuery(uint64_t handle) = 0;
    virtual void onWindowClipboardButtonPressed(uint64_t handle) = 0;
    virtual void onWindowDownloadCompleted(
        uint64_t handle, shared_ptr<CompletedDownload> file
    ) = 0;
    virtual bool onWindowStartFileUpload(uint64_t handle) = 0;
    virtual void onWindowCancelFileUpload(uint64_t handle) = 0;

    // To accept popup creation, the implementation should call the accept
    // function once with the handle of the new window as argument before
    // returning. The accept function returns the new window that uses the same
    // event handler as the original window, or empty if creation fails.
    virtual void onWindowCreatePopupRequest(
        uint64_t handle,
        function<shared_ptr<Window>(uint64_t)> accept
    ) = 0;
};

class Timeout;
class ViceFileUpload;

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
    public DownloadManagerEventHandler,
    public enable_shared_from_this<Window>
{
SHARED_ONLY_CLASS(Window);
public:
    // Returns empty pointer if CEF browser creation fails.
    static shared_ptr<Window> tryCreate(
        shared_ptr<WindowEventHandler> eventHandler,
        uint64_t handle,
        optional<string> uri
    );

    // Private constructor.
    Window(CKey, CKey);

    // Before destruction, ensure that the cleanup is complete.
    ~Window();

    void close();
    void resize(int width, int height);
    ImageSlice fetchViewImage();

    // -1 = back, 0 = refresh, 1 = forward.
    void navigate(int direction);

    void navigateToURI(string uri);

    void uploadFile(shared_ptr<ViceFileUpload> file);
    void cancelFileUpload();

    // Functions for passing input events to the Window. The functions accept
    // all combinations of argument values (the values are sanitized).
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
    virtual void onWidgetCursorChanged() override;
    virtual void onGlobalHotkeyPressed(GlobalHotkey key) override;

    // ControlBarEventHandler:
    virtual void onAddressSubmitted(string url) override;
    virtual void onQualityChanged(size_t idx) override;
    virtual void onPendingDownloadAccepted() override;
    virtual void onFind(string text, bool forward, bool findNext) override;
    virtual void onStopFind(bool clearSelection) override;
    virtual void onClipboardButtonPressed() override;
    virtual void onOpenBookmarksButtonPressed() override;
    virtual void onNavigationButtonPressed(int direction) override;
    virtual void onHomeButtonPressed() override;

    // BrowserAreaEventHandler:
    virtual void onBrowserAreaViewDirty() override;

    // DownloadManagerEventHandler:
    virtual void onPendingDownloadCountChanged(int count) override;
    virtual void onDownloadProgressChanged(vector<int> progress) override;
    virtual void onDownloadCompleted(
        shared_ptr<CompletedDownload> file
    ) override;

private:
    // Class that implements CefClient interfaces for the window.
    class Client;

    // To create a window:
    //   - Create the object using the private constructor.
    //   - Call init_().
    //   - Create a CEF browser using a Client object pointed to the window as
    //     the CefClient.
    //       - If creating the browser succeeded, call createSuccessful_();
    //         after this, the window is open.
    //       - If creating the browser failed, call createFailed_() and let the
    //         object destruct.
    void init_(shared_ptr<WindowEventHandler> eventHandler, uint64_t handle);
    void createSuccessful_();
    void createFailed_();

    void afterClose_();

    void watchdog_();
    void updateSecurityStatus_();

    void clampMouseCoords_(int& x, int& y);

    // May call onWindowViewImageChanged immediately.
    void signalImageChanged_();

    uint64_t handle_;
    enum {Open, Closed, CleanupComplete} state_;

    // Empty only in CleanupComplete state.
    shared_ptr<WindowEventHandler> eventHandler_;

    bool imageChanged_;

    // Always empty in CleanupComplete state. May be empty in Open and Closed
    // states if the browser has not yet started.
    CefRefPtr<CefBrowser> browser_;

    ImageSlice rootViewport_;
    shared_ptr<RootWidget> rootWidget_;

    shared_ptr<DownloadManager> downloadManager_;

    shared_ptr<Timeout> watchdogTimeout_;

    // The window is in file upload mode when fileUploadCallback_ is nonempty.
    CefRefPtr<CefFileDialogCallback> fileUploadCallback_;
    vector<shared_ptr<ViceFileUpload>> retainedUploads_;
};

}
