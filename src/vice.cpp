#include "vice.hpp"

#include <dlfcn.h>

VicePlugin::VicePlugin(CKey, CKey) {
    lib = nullptr;
}

VicePlugin::~VicePlugin() {
    if(lib != nullptr) {
        if(dlclose(lib) != 0) {
            LOG(WARNING) << "Unloading vice plugin failed";
        }
    }
}

shared_ptr<VicePlugin> VicePlugin::load(string filename) {
    shared_ptr<VicePlugin> plugin = VicePlugin::create(CKey());

    plugin->lib = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
    if(plugin->lib == nullptr) {
        LOG(ERROR) << "Loading vice plugin '" << filename << "' failed";
        return {};
    }

    void* sym;

    sym = dlsym(plugin->lib, "vicePlugin_isAPIVersionSupported");
    if(sym == nullptr) {
        LOG(ERROR) << "Loading symbol 'vicePlugin_isAPIVersionSupported' from vice plugin '" << filename << "' failed";
        return {};
    }
    plugin->vicePlugin_isAPIVersionSupported = (decltype(plugin->vicePlugin_isAPIVersionSupported))sym;

    sym = dlsym(plugin->lib, "vicePlugin_getConfigHelp");
    if(sym == nullptr) {
        LOG(ERROR) << "Loading symbol 'vicePlugin_getConfigHelp' from vice plugin '" << filename << "' failed";
        return {};
    }
    plugin->vicePlugin_getConfigHelp = (decltype(plugin->vicePlugin_getConfigHelp))sym;

    sym = dlsym(plugin->lib, "vicePlugin_initContext");
    if(sym == nullptr) {
        LOG(ERROR) << "Loading symbol 'vicePlugin_initContext' from vice plugin '" << filename << "' failed";
        return {};
    }
    plugin->vicePlugin_initContext = (decltype(plugin->vicePlugin_initContext))sym;

    plugin->apiVersion = 1000000;

    if(!plugin->vicePlugin_isAPIVersionSupported(plugin->apiVersion)) {
        LOG(ERROR) << "Vice plugin '" << filename << "' does not support API version " << plugin->apiVersion;
        return {};
    }

    return plugin;
}

vector<VicePlugin::ConfigHelpItem> VicePlugin::getConfigHelp() {
    vector<VicePlugin::ConfigHelpItem> ret;

    vicePlugin_getConfigHelp(
        apiVersion,
        [](void* data, const char* name, const char* valSpec, const char* desc, const char* defaultValStr) {
            vector<VicePlugin::ConfigHelpItem>& ret = *(vector<VicePlugin::ConfigHelpItem>*)data;
            ret.push_back({name, valSpec, desc, defaultValStr});
        },
        (void*)&ret
    );

    return ret;
}

ViceContext::ViceContext(CKey, CKey) {
    ctx = nullptr;
}

ViceContext::~ViceContext() {
    CHECK(ctx == nullptr);
}

shared_ptr<ViceContext> ViceContext::init(
    shared_ptr<VicePlugin> plugin,
    vector<pair<string, string>> options
) {
    shared_ptr<ViceContext> ctx = ViceContext::create(CKey());

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
        cerr << "ERROR: Vice plugin initialization failed" << msg << "\n";
    };

    ctx->ctx = plugin->vicePlugin_initContext(
        plugin->apiVersion,
        optionNames.data(),
        optionValues.data(),
        options.size(),
        [](void* initErrorMsgCallback, const char* msg) {
            (*(function<void(string)>*)initErrorMsgCallback)(msg);
        },
        &initErrorMsgCallback
    );
    if(ctx->ctx == nullptr) {
        if(!initErrorMsgCallbackCalled) {
            initErrorMsgCallback("");
        }
        return {};
    }

    return ctx;
}
