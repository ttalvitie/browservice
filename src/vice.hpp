#include "common.hpp"

// Dynamically loaded view service (vice) plugin library that shows the browser
// view to the user and relays back the input events. Can be interacted with
// using a ViceContext.
class VicePlugin {
SHARED_ONLY_CLASS(VicePlugin);
public:
    VicePlugin(CKey, CKey);
    ~VicePlugin();

    // Returns an empty pointer if loading the plugin failed.
    static shared_ptr<VicePlugin> load(string filename);

    struct OptionHelpItem {
        string name;
        string valSpec;
        string desc;
        string defaultValStr;
    };
    vector<OptionHelpItem> getOptionHelp();

private:
    string filename_;
    void* lib_;
    uint64_t apiVersion_;

    struct APIFuncs;
    unique_ptr<APIFuncs> apiFuncs_;

    friend class ViceContext;
};

typedef struct VicePluginAPI_Context VicePluginAPI_Context;

// An initialized VicePlugin context.
class ViceContext {
SHARED_ONLY_CLASS(ViceContext);
public:
    ViceContext(CKey, CKey);
    ~ViceContext();

    // Returns an empty pointer if starting the plugin failed. The reason for the
    // failure is written to stderr.
    static shared_ptr<ViceContext> init(
        shared_ptr<VicePlugin> plugin,
        vector<pair<string, string>> options
    );

private:
    shared_ptr<VicePlugin> plugin_;
    VicePluginAPI_Context* handle_;
};
