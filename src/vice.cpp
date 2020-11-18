#include "vice.hpp"

#include "../vice_plugin_api.h"

#include <dlfcn.h>

struct VicePlugin::APIFuncs {
#define FOREACH_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isAPIVersionSupported) \
    FOREACH_VICE_API_FUNC_ITEM(getOptionHelp) \
    FOREACH_VICE_API_FUNC_ITEM(initContext) \
    FOREACH_VICE_API_FUNC_ITEM(startContext) \
    FOREACH_VICE_API_FUNC_ITEM(asyncStopContext) \
    FOREACH_VICE_API_FUNC_ITEM(destroyContext)

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    decltype(&vicePluginAPI_ ## name) name;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM
};

shared_ptr<VicePlugin> VicePlugin::load(string filename) {
    REQUIRE_UI_THREAD();

    void* lib = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if(lib == nullptr) {
        ERROR_LOG("Loading vice plugin '", filename, "' failed");
        return {};
    }

    unique_ptr<APIFuncs> apiFuncs = make_unique<APIFuncs>();

    void* sym;

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    sym = dlsym(lib, "vicePluginAPI_" #name); \
    if(sym == nullptr) { \
        ERROR_LOG( \
            "Loading symbol 'vicePluginAPI_" #name "' from vice plugin '", \
            filename, "' failed" \
        ); \
        REQUIRE(dlclose(lib) == 0); \
        return {}; \
    } \
    apiFuncs->name = (decltype(apiFuncs->name))sym;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM

    uint64_t apiVersion = 1000000;

    if(!apiFuncs->isAPIVersionSupported(apiVersion)) {
        ERROR_LOG(
            "Vice plugin '", filename,
            "' does not support API version ", apiVersion
        );
        REQUIRE(dlclose(lib) == 0);
        return {};
    }

    return VicePlugin::create(
        CKey(),
        filename,
        lib,
        apiVersion,
        move(apiFuncs)
    );
}

vector<VicePlugin::OptionHelpItem> VicePlugin::getOptionHelp() {
    REQUIRE_UI_THREAD();

    vector<VicePlugin::OptionHelpItem> ret;

    apiFuncs_->getOptionHelp(
        apiVersion_,
        [](void* data, const char* name, const char* valSpec, const char* desc, const char* defaultValStr) {
            vector<VicePlugin::OptionHelpItem>& ret = *(vector<VicePlugin::OptionHelpItem>*)data;
            ret.push_back({name, valSpec, desc, defaultValStr});
        },
        (void*)&ret
    );

    return ret;
}

VicePlugin::VicePlugin(CKey, CKey,
    string filename,
    void* lib,
    uint64_t apiVersion,
    unique_ptr<APIFuncs> apiFuncs
) {
    filename_ = filename;
    lib_ = lib;
    apiVersion_ = apiVersion;
    apiFuncs_ = move(apiFuncs);
}

VicePlugin::~VicePlugin() {
    REQUIRE(dlclose(lib_) == 0);
}

shared_ptr<ViceContext> ViceContext::init(
    shared_ptr<VicePlugin> plugin,
    vector<pair<string, string>> options
) {
    REQUIRE_UI_THREAD();

    vector<const char*> optionNames;
    vector<const char*> optionValues;
    for(const pair<string, string>& opt : options) {
        optionNames.push_back(opt.first.c_str());
        optionValues.push_back(opt.second.c_str());
    }

    bool initErrorMsgCallbackCalled = false;
    function<void(string)> initErrorMsgCallback = [&](string msg) {
        initErrorMsgCallbackCalled = true;
        if(!msg.empty()) {
            msg = ": " + msg;
        }
        cerr
            << "ERROR: Vice plugin '" << plugin->filename_ <<
            "' initialization failed" << msg << "\n";
    };

    VicePluginAPI_Context* handle = plugin->apiFuncs_->initContext(
        plugin->apiVersion_,
        optionNames.data(),
        optionValues.data(),
        options.size(),
        [](void* initErrorMsgCallback, const char* msg) {
            (*(function<void(string)>*)initErrorMsgCallback)(msg);
        },
        &initErrorMsgCallback,
        [](void* plugin, const char* location, const char* msg) {
            Panicker(((VicePlugin*)plugin)->filename_ + " " + location)(msg);
        },
        plugin.get(),
        [](void* plugin, int severity, const char* location, const char* msg) {
            const char* severityStr;
            if(severity == 2) {
                severityStr = "ERROR";
            } else if(severity == 1) {
                severityStr = "WARNING";
            } else {
                if(severity != 0) {
                    WARNING_LOG(
                        "Vice plugin log severity ", severity,
                        " unknown, defaulting to INFO"
                    );
                }
                severityStr = "INFO";
            }
            LogWriter(
                severityStr,
                ((VicePlugin*)plugin)->filename_ + " " + location
            )(msg);
        },
        plugin.get()
    );
    if(handle == nullptr) {
        if(!initErrorMsgCallbackCalled) {
            initErrorMsgCallback("");
        }
        return {};
    }

    return ViceContext::create(CKey(), plugin, handle);
}

ViceContext::ViceContext(CKey, CKey,
    shared_ptr<VicePlugin> plugin,
    VicePluginAPI_Context* handle
) {
    plugin_ = plugin;
    handle_ = handle;
    startedBefore_ = false;
    running_ = false;
    stopping_ = false;
}

ViceContext::~ViceContext() {
    REQUIRE(!running_);
    plugin_->apiFuncs_->destroyContext(handle_);
}

void ViceContext::start(weak_ptr<ViceContextEventHandler> eventHandler) {
    REQUIRE_UI_THREAD();
    REQUIRE(!startedBefore_);

    running_ = true;
    startedBefore_ = true;
    eventHandler_ = eventHandler;
    selfLoop_ = shared_from_this();

    INFO_LOG("Starting vice plugin ", plugin_->filename_);

    plugin_->apiFuncs_->startContext(handle_);
}

void ViceContext::asyncStop() {
    REQUIRE_UI_THREAD();
    REQUIRE(running_);
    REQUIRE(!stopping_);

    stopping_ = true;

    INFO_LOG("Stopping vice plugin ", plugin_->filename_);
    plugin_->apiFuncs_->asyncStopContext(
        handle_,
        [](void* self) {
            postTask(
                ((ViceContext*)self)->shared_from_this(),
                &ViceContext::asyncStopComplete_
            );
        },
        this
    );
}

bool ViceContext::isRunning() {
    REQUIRE_UI_THREAD();
    return running_;
}

void ViceContext::asyncStopComplete_() {
    REQUIRE_UI_THREAD();
    REQUIRE(running_);
    REQUIRE(stopping_);

    stopping_ = false;
    running_ = false;
    selfLoop_.reset();

    INFO_LOG("Vice plugin ", plugin_->filename_, " stopped successfully");

    postTask(eventHandler_, &ViceContextEventHandler::onViceContextStopComplete);
}
