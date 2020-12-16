#pragma once

#include "common.hpp"

// Dynamically loaded view service (vice) plugin library that shows the browser
// view to the user and relays back the input events. Can be interacted with
// using a ViceContext.
class VicePlugin {
SHARED_ONLY_CLASS(VicePlugin);
private:
    struct APIFuncs;

public:
    // Returns an empty pointer if loading the plugin failed.
    static shared_ptr<VicePlugin> load(string filename);

    // Private constructor.
    VicePlugin(CKey, CKey,
        string filename,
        void* lib,
        uint64_t apiVersion,
        unique_ptr<APIFuncs> apiFuncs
    );
    ~VicePlugin();

    string getVersionString();

    struct OptionDocsItem {
        string name;
        string valSpec;
        string desc;
        string defaultValStr;
    };
    vector<OptionDocsItem> getOptionDocs();

private:
    string filename_;
    void* lib_;
    uint64_t apiVersion_;

    unique_ptr<APIFuncs> apiFuncs_;

    friend class ViceContext;
};

class ViceContextEventHandler {
public:
    virtual void onViceContextShutdownComplete() = 0;
};

typedef struct VicePluginAPI_Context VicePluginAPI_Context;

// An initialized vice plugin context.
class ViceContext : public enable_shared_from_this<ViceContext> {
SHARED_ONLY_CLASS(ViceContext);
public:
    // Returns an empty pointer if initializing the plugin failed. The reason
    // for the failure is written to stderr.
    static shared_ptr<ViceContext> init(
        shared_ptr<VicePlugin> plugin,
        vector<pair<string, string>> options
    );

    // Private constructor.
    ViceContext(CKey, CKey,
        shared_ptr<VicePlugin> plugin,
        VicePluginAPI_Context* handle
    );
    ~ViceContext();

    // Start running the context. Before quitting CEF message loop, call
    // shutdown and wait for onViceContextShutdownComplete event.
    void start(weak_ptr<ViceContextEventHandler> eventHandler);
    void shutdown();

    bool isShutdownComplete();

private:
    static shared_ptr<ViceContext> getContext_(void* callbackData);

    void pumpEvents_();
    void shutdownComplete_();

    shared_ptr<VicePlugin> plugin_;
    VicePluginAPI_Context* ctx_;

    enum {Pending, Running, ShutdownComplete} state_;
    bool shutdownPending_;
    atomic<bool> pumpEventsInQueue_;

    weak_ptr<ViceContextEventHandler> eventHandler_;

    // For running contexts, we store a shared pointer to self to avoid it being
    // destructed.
    shared_ptr<ViceContext> self_;

    void* callbackData_;

    set<uint64_t> openWindows_;
};
