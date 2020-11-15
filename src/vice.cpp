#include "vice.hpp"

#include "../vice_plugin_api.h"

#include <dlfcn.h>

struct VicePlugin::APIFuncs {
#define FOREACH_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isAPIVersionSupported) \
    FOREACH_VICE_API_FUNC_ITEM(getOptionHelp) \
    FOREACH_VICE_API_FUNC_ITEM(initContext) \
    FOREACH_VICE_API_FUNC_ITEM(destroyContext)

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    decltype(&vicePluginAPI_ ## name) name;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM
};

VicePlugin::VicePlugin(CKey, CKey) {
    lib_ = nullptr;
}

VicePlugin::~VicePlugin() {
    if(lib_ != nullptr) {
        if(dlclose(lib_) != 0) {
            LOG(WARNING) << "Unloading vice plugin failed";
        }
    }
}

shared_ptr<VicePlugin> VicePlugin::load(string filename) {
    shared_ptr<VicePlugin> plugin = VicePlugin::create(CKey());

    plugin->filename_ = filename;
    plugin->lib_ = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if(plugin->lib_ == nullptr) {
        LOG(ERROR) << "Loading vice plugin '" << filename << "' failed";
        return {};
    }

    plugin->apiFuncs_ = make_unique<APIFuncs>();

    void* sym;

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    sym = dlsym(plugin->lib_, "vicePluginAPI_" #name); \
    if(sym == nullptr) { \
        LOG(ERROR) \
            << "Loading symbol 'vicePluginAPI_" #name "' from vice plugin '" \
            << filename << "' failed"; \
        return {}; \
    } \
    plugin->apiFuncs_->name = (decltype(plugin->apiFuncs_->name))sym;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM

    plugin->apiVersion_ = 1000000;

    if(!plugin->apiFuncs_->isAPIVersionSupported(plugin->apiVersion_)) {
        LOG(ERROR) << "Vice plugin '" << filename << "' does not support API version " << plugin->apiVersion_;
        return {};
    }

    return plugin;
}

vector<VicePlugin::OptionHelpItem> VicePlugin::getOptionHelp() {
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

ViceContext::ViceContext(CKey, CKey) {
    handle_ = nullptr;
}

ViceContext::~ViceContext() {
    if(handle_ != nullptr) {
        plugin_->apiFuncs_->destroyContext(handle_);
    }
}

shared_ptr<ViceContext> ViceContext::init(
    shared_ptr<VicePlugin> plugin,
    vector<pair<string, string>> options
) {
    shared_ptr<ViceContext> ctx = ViceContext::create(CKey());
    ctx->plugin_ = plugin;

    vector<const char*> optionNames;
    vector<const char*> optionValues;
    for(const pair<string, string>& opt : options) {
        optionNames.push_back(opt.first.c_str());
        optionValues.push_back(opt.first.c_str());
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

    ctx->handle_ = plugin->apiFuncs_->initContext(
        plugin->apiVersion_,
        optionNames.data(),
        optionValues.data(),
        options.size(),
        [](void* initErrorMsgCallback, const char* msg) {
            (*(function<void(string)>*)initErrorMsgCallback)(msg);
        },
        &initErrorMsgCallback,
        [](void* self, const char* location, const char* msg) {
            Panicker(((ViceContext*)self)->plugin_->filename_ + " " + location)(msg);
        },
        ctx.get(),
        [](void* self, int severity, const char* location, const char* msg) {
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
                ((ViceContext*)self)->plugin_->filename_ + " " + location
            )(msg);
        },
        ctx.get()
    );
    if(ctx->handle_ == nullptr) {
        if(!initErrorMsgCallbackCalled) {
            initErrorMsgCallback("");
        }
        return {};
    }

    return ctx;
}
