#pragma once

#include "window.hpp"

namespace retrojsvice {

class WindowManagerEventHandler {
public:
    virtual variant<uint64_t, string> onWindowManagerCreateWindowRequest() = 0;
    virtual variant<uint64_t, string> onWindowManagerCreateWindowWithURIRequest(string uri) = 0;
    virtual void onWindowManagerCloseWindow(uint64_t window) = 0;

    // See ImageCompressorEventHandler::onImageCompressorFetchImage
    virtual void onWindowManagerFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;

    virtual void onWindowManagerResizeWindow(
        uint64_t window,
        size_t width,
        size_t height
    ) = 0;

    virtual void onWindowManagerMouseDown(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onWindowManagerMouseUp(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onWindowManagerMouseMove(uint64_t window, int x, int y) = 0;
    virtual void onWindowManagerMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onWindowManagerMouseWheel(
        uint64_t window, int x, int y, int delta
    ) = 0;
    virtual void onWindowManagerMouseLeave(uint64_t window, int x, int y) = 0;

    virtual void onWindowManagerKeyDown(uint64_t window, int key) = 0;
    virtual void onWindowManagerKeyUp(uint64_t window, int key) = 0;

    virtual void onWindowManagerLoseFocus(uint64_t window) = 0;

    virtual void onWindowManagerNavigate(uint64_t window, int direction) = 0;
    virtual void onWindowManagerNavigateToURI(uint64_t window, string uri) = 0;

    virtual void onWindowManagerUploadFile(
        uint64_t window, string name, shared_ptr<FileUpload> file
    ) = 0;
    virtual void onWindowManagerCancelFileUpload(uint64_t window) = 0;
};

class FileDownload;
class HTTPRequest;
class SecretGenerator;

// Must be closed with close() prior to destruction.
class WindowManager :
    public WindowEventHandler,
    public enable_shared_from_this<WindowManager>
{
SHARED_ONLY_CLASS(WindowManager);
public:
    WindowManager(CKey,
        shared_ptr<WindowManagerEventHandler> eventHandler,
        shared_ptr<SecretGenerator> secretGen,
        string programName,
        int defaultQuality
    );
    ~WindowManager();

    // Immediately closes all windows and prevents new windows from being
    // created; new HTTP requests are dropped immediately. May call
    // WindowManagerEventHandler::onCloseWindow directly and drops shared
    // pointer to event handler. Will not call any other event handlers.
    void close(MCE);

    void handleHTTPRequest(MCE, shared_ptr<HTTPRequest> request);

    bool createPopupWindow(
        uint64_t parentWindow,
        uint64_t popupWindow,
        string& reason
    );
    void closeWindow(uint64_t window);
    void notifyViewChanged(uint64_t window);

    void setCursor(uint64_t window, int cursorSignal);

    optional<pair<vector<string>, size_t>> qualitySelectorQuery(
        uint64_t window
    );
    void qualityChanged(uint64_t window, size_t qualityIdx);

    bool needsClipboardButtonQuery(uint64_t window);
    void clipboardButtonPressed(uint64_t window);

    void putFileDownload(uint64_t window, shared_ptr<FileDownload> file);

    bool startFileUpload(uint64_t window);
    void cancelFileUpload(uint64_t window);

    // WindowEventHandler:
    virtual void onWindowClose(uint64_t window) override;
    virtual void onWindowFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) override;
    virtual void onWindowResize(
        uint64_t window,
        size_t width,
        size_t height
    ) override;
    virtual void onWindowMouseDown(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowMouseUp(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowMouseMove(uint64_t window, int x, int y) override;
    virtual void onWindowMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowMouseWheel(
        uint64_t window, int x, int y, int delta
    ) override;
    virtual void onWindowMouseLeave(uint64_t window, int x, int y) override;
    virtual void onWindowKeyDown(uint64_t window, int key) override;
    virtual void onWindowKeyUp(uint64_t window, int key) override;
    virtual void onWindowLoseFocus(uint64_t window) override;
    virtual void onWindowNavigate(uint64_t window, int direction) override;
    virtual void onWindowNavigateToURI(uint64_t window, string uri) override;
    virtual void onWindowUploadFile(
        uint64_t window, string name, shared_ptr<FileUpload> file
    ) override;
    virtual void onWindowCancelFileUpload(uint64_t window) override;

private:
    void handleNewWindowRequest_(MCE, shared_ptr<HTTPRequest> request, optional<string> uri);

    shared_ptr<WindowManagerEventHandler> eventHandler_;
    bool closed_;

    map<uint64_t, shared_ptr<Window>> windows_;

    shared_ptr<SecretGenerator> secretGen_;
    string programName_;
    int defaultQuality_;
};

}
