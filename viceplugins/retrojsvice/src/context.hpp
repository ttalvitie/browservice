#pragma once

#include "http.hpp"
#include "window_manager.hpp"

#include "vice_plugin_api.hpp"

namespace retrojsvice {

class SecretGenerator;

// The implementation of the vice plugin context, exposed through the C API in
// vice_plugin_api.cpp.
class Context :
    public HTTPServerEventHandler,
    public TaskQueueEventHandler,
    public WindowManagerEventHandler,
    public enable_shared_from_this<Context>
{
SHARED_ONLY_CLASS(Context);
public:
    // Returns either a successfully constructed context or an error message.
    static variant<shared_ptr<Context>, string> init(
        vector<pair<string, string>> options,
        string programName
    );

    // Private constructor.
    Context(CKey, CKey,
        int defaultQuality,
        SocketAddress httpListenAddr,
        int httpMaxThreads,
        string httpAuthCredentials,
        bool allowQualitySelector,
        string programName
    );
    ~Context();

    // Public API functions:
    void URINavigation_enable(VicePluginAPI_URINavigation_Callbacks callbacks);

    void start(
        VicePluginAPI_Callbacks callbacks,
        void* callbackData
    );
    void shutdown();

    void pumpEvents();

    int createPopupWindow(
        uint64_t parentWindow,
        uint64_t popupWindow,
        char** msg
    );
    void closeWindow(uint64_t window);
    void notifyWindowViewChanged(uint64_t window);

    void setWindowCursor(uint64_t window, VicePluginAPI_MouseCursor cursor);

    int windowQualitySelectorQuery(
        uint64_t window,
        char** qualityListOut,
        size_t* currentQualityOut
    );
    void windowQualityChanged(uint64_t window, size_t qualityIdx);

    int windowNeedsClipboardButtonQuery(uint64_t window);
    void windowClipboardButtonPressed(uint64_t window);
    void putClipboardContent(const char* text);

    void putFileDownload(
        uint64_t window,
        const char* name,
        const char* path,
        void (*cleanup)(void*),
        void* cleanupData
    );

    int startFileUpload(uint64_t window);
    void cancelFileUpload(uint64_t window);

    // Returns (name, valSpec, desc, defaultValStr)-tuples.
    static vector<tuple<string, string, string, string>> getOptionDocs();

    // HTTPServerEventHandler:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request) override;
    virtual void onHTTPServerShutdownComplete() override;

    // TaskQueueEventHandler:
    virtual void onTaskQueueNeedsRunTasks() override;
    virtual void onTaskQueueShutdownComplete() override;

    // WindowManagerEventHandler;
    virtual variant<uint64_t, string> onWindowManagerCreateWindowRequest() override;
    virtual variant<uint64_t, string> onWindowManagerCreateWindowWithURIRequest(string uri) override;
    virtual void onWindowManagerCloseWindow(uint64_t window) override;
    virtual void onWindowManagerFetchImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) override;
    virtual void onWindowManagerResizeWindow(
        uint64_t window,
        size_t width,
        size_t height
    ) override;
    virtual void onWindowManagerMouseDown(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowManagerMouseUp(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowManagerMouseMove(
        uint64_t window, int x, int y
    ) override;
    virtual void onWindowManagerMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) override;
    virtual void onWindowManagerMouseWheel(
        uint64_t window, int x, int y, int delta
    ) override;
    virtual void onWindowManagerMouseLeave(
        uint64_t window, int x, int y
    ) override;
    virtual void onWindowManagerKeyDown(uint64_t window, int key) override;
    virtual void onWindowManagerKeyUp(uint64_t window, int key) override;
    virtual void onWindowManagerLoseFocus(uint64_t window) override;
    virtual void onWindowManagerNavigate(
        uint64_t window, int direction
    ) override;
    virtual void onWindowManagerNavigateToURI(
        uint64_t window, string uri
    ) override;
    virtual void onWindowManagerUploadFile(
        uint64_t window, string name, shared_ptr<FileUpload> file
    ) override;
    virtual void onWindowManagerCancelFileUpload(uint64_t window) override;

private:
    void handleClipboardHTTPRequest_(MCE, shared_ptr<HTTPRequest> request);
    void startClipboardTimeout_();

    int defaultQuality_;
    SocketAddress httpListenAddr_;
    int httpMaxThreads_;
    string httpAuthCredentials_;
    bool allowQualitySelector_;
    string programName_;

    enum {Pending, Running, ShutdownComplete} state_;
    enum {
        NoPendingShutdown,
        WaitWindowManager,
        WaitHTTPServer,
        WaitTaskQueue
    } shutdownPhase_;
    atomic<bool> inAPICall_;

    VicePluginAPI_Callbacks callbacks_;
    void* callbackData_;

    optional<VicePluginAPI_URINavigation_Callbacks> uriNavigationCallbacks_;

    shared_ptr<TaskQueue> taskQueue_;
    shared_ptr<HTTPServer> httpServer_;
    shared_ptr<SecretGenerator> secretGen_;
    shared_ptr<WindowManager> windowManager_;

    string clipboardCSRFToken_;
    vector<shared_ptr<HTTPRequest>> clipboardRequests_;
    shared_ptr<DelayedTaskTag> clipboardTimeout_;

    class APILock;
    class RunningAPILock;
};

}
