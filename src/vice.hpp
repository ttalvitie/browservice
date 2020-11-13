#include "common.hpp"

// Dynamically loaded view service (vice) plugin library that shows the browser
// view to the user and relays back the input events. Used mostly through
// ViceContext.
class VicePlugin {
SHARED_ONLY_CLASS(VicePlugin);
public:
    VicePlugin(CKey, CKey);
    ~VicePlugin();

    // Returns an empty pointer if loading the plugin failed.
    static shared_ptr<VicePlugin> load(string filename);

    struct ConfigHelpItem {
        string name;
        string valSpec;
        string desc;
        string defaultValStr;
    };
    vector<ConfigHelpItem> getConfigHelp();

private:
    void* lib;

    uint64_t apiVersion;

    int (*vicePlugin_isAPIVersionSupported)(uint64_t);
    void (*vicePlugin_getConfigHelp)(
        uint64_t,
        void (*)(void*, const char*, const char*, const char*, const char*),
        void*
    );
    void* (*vicePlugin_initContext)(
        uint64_t,
        const char**,
        const char**,
        size_t,
        void (*)(void*, const char*),
        void*
    );

    friend class ViceContext;
};

// An initialized VicePlugin context.
// TODO: correct shutdown
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
    void* ctx;
};
