#pragma once

#include "image_compressor.hpp"
#include "task_queue.hpp"

namespace retrojsvice {

class FileUpload {
SHARED_ONLY_CLASS(FileUpload);
public:
    FileUpload(CKey, string path) {
        path_ = path;
    }
    ~FileUpload() {
        INFO_LOG("DESTROYED");
    }

    string path() {
        return path_;
    }

private:
    string path_;
};

class WindowEventHandler {
public:
    // Called when window closes itself (i.e. is not closed by a call to
    // Window::close()). The window is immediately closed as if Window::close
    // was called.
    virtual void onWindowClose(uint64_t window) = 0;

    // See ImageCompressorEventHandler::onImageCompressorFetchImage
    virtual void onWindowFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;

    virtual void onWindowResize(
        uint64_t window,
        size_t width,
        size_t height
    ) = 0;

    virtual void onWindowMouseDown(uint64_t window, int x, int y, int button) = 0;
    virtual void onWindowMouseUp(uint64_t window, int x, int y, int button) = 0;
    virtual void onWindowMouseMove(uint64_t window, int x, int y) = 0;
    virtual void onWindowMouseDoubleClick(uint64_t window, int x, int y, int button) = 0;
    virtual void onWindowMouseWheel(uint64_t window, int x, int y, int delta) = 0;
    virtual void onWindowMouseLeave(uint64_t window, int x, int y) = 0;

    virtual void onWindowKeyDown(uint64_t window, int key) = 0;
    virtual void onWindowKeyUp(uint64_t window, int key) = 0;

    virtual void onWindowLoseFocus(uint64_t window) = 0;

    virtual void onWindowNavigate(uint64_t window, int direction) = 0;

    virtual void onWindowUploadFile(
        uint64_t window, shared_ptr<FileUpload> file
    ) = 0;
    virtual void onWindowCancelFileUpload(uint64_t window) = 0;
};

class FileDownload;
class HTTPRequest;
class SecretGenerator;

// Must be closed before destruction (as signaled by the onWindowClose, caused
// by the Window itself or initiated using Window::close)
class Window :
    public ImageCompressorEventHandler,
    public enable_shared_from_this<Window>
{
SHARED_ONLY_CLASS(Window);
public:
    Window(CKey,
        shared_ptr<WindowEventHandler> eventHandler,
        uint64_t handle,
        shared_ptr<SecretGenerator> secretGen,
        string programName,
        bool allowPNG,
        int initialQuality
    );
    ~Window();

    // Immediately closes the window (no more event handlers will be called and
    // no member functions may be called for this window). Does not call
    // WindowEventHandler::onWindowClose.
    void close();

    void handleInitialForwardHTTPRequest(shared_ptr<HTTPRequest> request);
    void handleHTTPRequest(MCE, shared_ptr<HTTPRequest> request);

    shared_ptr<Window> createPopup(uint64_t popupHandle);

    void notifyViewChanged();

    void setCursor(int cursorSignal);

    optional<pair<vector<string>, size_t>> qualitySelectorQuery();
    void qualityChanged(size_t qualityIdx);

    void clipboardButtonPressed();

    void putFileDownload(shared_ptr<FileDownload> file);

    bool startFileUpload();
    void cancelFileUpload();

    // ImageCompressorEventHandler:
    virtual void onImageCompressorFetchImage(
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) override;
    virtual void onImageCompressorRenderGUI(
        vector<uint8_t>& data, size_t width, size_t height
    ) override;

private:
    void afterConstruct_(shared_ptr<Window> self);

    // Closes window and calls WindowEventHandler::onWindowClose.
    void selfClose_(MCE);

    void updateInactivityTimeout_(bool shorten = false);
    void inactivityTimeoutReached_(MCE, bool shortened);

    int decodeKey_(uint64_t eventIdx, int key);
    bool handleTokenizedEvent_(MCE,
        uint64_t eventIdx,
        const string& name,
        int argCount,
        const int* args
    );
    bool handleEvent_(MCE,
        uint64_t eventIdx,
        string::const_iterator begin,
        string::const_iterator end
    );
    void handleEvents_(MCE, uint64_t startIdx, string eventStr);

    void navigate_(MCE, int direction);

    void handleMainPageRequest_(MCE, shared_ptr<HTTPRequest> request);
    void handleImageRequest_(MCE,
        shared_ptr<HTTPRequest> request,
        uint64_t mainIdx,
        uint64_t imgIdx,
        int immediate,
        int width,
        int height,
        uint64_t startEventIdx,
        string eventStr
    );
    void handleIframeRequest_(MCE,
        shared_ptr<HTTPRequest> request,
        uint64_t mainIdx
    );
    void handleCloseRequest_(
        shared_ptr<HTTPRequest> request,
        uint64_t mainIdx
    );
    void handlePrevPageRequest_(MCE, shared_ptr<HTTPRequest> request);
    void handleNextPageRequest_(MCE, shared_ptr<HTTPRequest> request);

    void addIframe_(MCE, function<void(shared_ptr<HTTPRequest>)> iframe);

    string programName_;
    bool allowPNG_;
    int initialQuality_;
    shared_ptr<SecretGenerator> secretGen_;

    // The key codes sent by the client are XOR "encrypted" using this key. Note
    // that THIS DOES NOT PROVIDE SECURITY from sniffers, because the key is
    // sent in plain text in the HTML. The only point of this is to reduce the
    // likelihood that a password being typed is revealed from the URL in the
    // status bar of the browser for example in screen capture videos. Even this
    // does not always work due to the inherent and grave vulnerability of the
    // non-OTP XOR encryption. Thus you should NEVER rely on this providing any
    // kind of security.
    vector<int> snakeOilKeyCipherKey_;

    shared_ptr<WindowEventHandler> eventHandler_;
    uint64_t handle_;
    string csrfToken_;
    string pathPrefix_;
    bool closed_;

    shared_ptr<ImageCompressor> imageCompressor_;
    shared_ptr<DelayedTaskTag> animationTag_;

    int width_;
    int height_;

    set<int> mouseButtonsDown_;
    set<int> keysDown_;

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

    // Downloads whose iframe has been loaded; the actual file is kept available
    // until a timeout has expired.
    map<
        uint64_t,
        pair<shared_ptr<FileDownload>, shared_ptr<DelayedTaskTag>>
    > downloads_;
    uint64_t curDownloadIdx_;

    shared_ptr<DelayedTaskTag> inactivityTimeoutTag_;

    steady_clock::time_point lastNavigateOperationTime_;

    queue<function<void(shared_ptr<HTTPRequest>)>> iframeQueue_;

    bool inFileUploadMode_;
};

}
