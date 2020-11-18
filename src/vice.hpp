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

    struct OptionHelpItem {
        string name;
        string valSpec;
        string desc;
        string defaultValStr;
    };
    vector<OptionHelpItem> getOptionHelp();

    VicePlugin(CKey, CKey,
        string filename,
        void* lib,
        uint64_t apiVersion,
        unique_ptr<APIFuncs> apiFuncs
    );
    ~VicePlugin();

private:
    string filename_;
    void* lib_;
    uint64_t apiVersion_;

    unique_ptr<APIFuncs> apiFuncs_;

    friend class ViceContext;
};

typedef struct VicePluginAPI_Context VicePluginAPI_Context;

class ViceContextEventHandler {
public:
    virtual void onViceContextStopComplete() = 0;
};

// An initialized vice plugin context.
class ViceContext : public enable_shared_from_this<ViceContext> {
SHARED_ONLY_CLASS( ViceContext);
public:
    // Returns an empty pointer if initializing the plugin failed. The reason
    // for the failure is written to stderr.
    static shared_ptr<ViceContext> init(
        shared_ptr<VicePlugin> plugin,
        vector<pair<string, string>> options
    );

    ViceContext(CKey, CKey,
        shared_ptr<VicePlugin> plugin,
        VicePluginAPI_Context* handle
    );
    ~ViceContext();

    // To start the context, CEF event loop must be running. The context may be
    // started only once. Before ending CEF event loop, the context must be
    // stopped by calling asyncStop() and waiting for onViceContextStopComplete
    // to be called.
    void start(weak_ptr<ViceContextEventHandler> eventHandler);
    void asyncStop();

    // Returns true if the context is running, that is, either start has not
    // been called or asyncStop has finished successfully.
    bool isRunning();

private:
    void asyncStopComplete_();

    shared_ptr<VicePlugin> plugin_;
    VicePluginAPI_Context* handle_;

    bool startedBefore_;
    bool running_;
    bool stopping_;
    weak_ptr<ViceContextEventHandler> eventHandler_;

    // While the context is started, we keep the object alive using a reference
    // cycle.
    shared_ptr<ViceContext> selfLoop_;
};
