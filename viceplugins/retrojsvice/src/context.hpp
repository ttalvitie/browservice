#pragma once

#include "http.hpp"
#include "window.hpp"

namespace retrojsvice {

// The implementation of the vice plugin context, exposed through the C API in
// vice_plugin_api.cpp.
class Context :
    public HTTPServerEventHandler,
    public TaskQueueEventHandler,
    public enable_shared_from_this<Context>
{
SHARED_ONLY_CLASS(Context);
public:
    // Returns either a successfully constructed context or an error message.
    static variant<shared_ptr<Context>, string> init(
        vector<pair<string, string>> options
    );

    // Private constructor.
    Context(CKey, CKey,
        SocketAddress httpListenAddr,
        int httpMaxThreads,
        string httpAuthCredentials
    );
    ~Context();

    // Public API functions:
    void start(
        void (*eventNotifyCallback)(void*),
        void (*shutdownCompleteCallback)(void*),
        void* callbackData
    );
    void shutdown();

    void pumpEvents();

    void setWindowCallbacks(
        uint64_t (*createWindowCallback)(void*, char** msg),
        void (*closeWindowCallback)(void*, uint64_t handle),
        void (*resizeWindowCallback)(void*, uint64_t handle, int width, int height)
    );

    void closeWindow(uint64_t handle);

    // Returns (name, valSpec, desc, defaultValStr)-tuples.
    static vector<tuple<string, string, string, string>> getOptionDocs();

    // HTTPServerEventHandler:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request) override;
    virtual void onHTTPServerShutdownComplete() override;

    // TaskQueueEventHandler:
    virtual void onTaskQueueNeedsRunTasks() override;
    virtual void onTaskQueueShutdownComplete() override;

private:
    void handleNewWindowRequest_(shared_ptr<HTTPRequest> request);

    SocketAddress httpListenAddr_;
    int httpMaxThreads_;
    string httpAuthCredentials_;

    enum {Pending, Running, ShutdownComplete} state_;
    enum {NoPendingShutdown, WaitHTTPServer, WaitTaskQueue} shutdownPhase_;
    atomic<bool> inAPICall_;

    void* callbackData_;

    void (*eventNotifyCallback_)(void*);
    void (*shutdownCompleteCallback_)(void*);

    uint64_t (*createWindowCallback_)(void*, char** msg);
    void (*closeWindowCallback_)(void*, uint64_t handle);
    void (*resizeWindowCallback_)(void*, uint64_t handle, int width, int height);

    shared_ptr<TaskQueue> taskQueue_;
    shared_ptr<HTTPServer> httpServer_;

    map<uint64_t, shared_ptr<Window>> windows_;

    class APILock;
    class RunningAPILock;
};

}
