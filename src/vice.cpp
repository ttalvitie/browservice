#include "vice.hpp"

#include "../vice_plugin_api.h"

#include <dlfcn.h>

struct VicePlugin::APIFuncs {
#define FOREACH_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isAPIVersionSupported) \
    FOREACH_VICE_API_FUNC_ITEM(getVersionString) \
    FOREACH_VICE_API_FUNC_ITEM(initContext) \
    FOREACH_VICE_API_FUNC_ITEM(destroyContext) \
    FOREACH_VICE_API_FUNC_ITEM(getOptionDocs) \
    FOREACH_VICE_API_FUNC_ITEM(setLogCallback) \
    FOREACH_VICE_API_FUNC_ITEM(setPanicCallback)

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    decltype(&vicePluginAPI_ ## name) name;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM
};

namespace {

#define API_CALLBACK_START try {
#define API_CALLBACK_END \
    } catch(const exception& e) { \
        PANIC("Unhandled exception traversing the vice plugin API: ", e.what()); \
    } catch(...) { \
        PANIC("Unhandled exception traversing the vice plugin API"); \
    }

void logCallback(
    void* filenamePtr,
    int logLevel,
    const char* location,
    const char* msg
) {
API_CALLBACK_START

    const string& filename = *(string*)filenamePtr;

    const char* logLevelStr;
    if(logLevel == VICE_PLUGIN_API_LOG_LEVEL_ERROR) {
        logLevelStr = "ERROR";
    } else if(logLevel == VICE_PLUGIN_API_LOG_LEVEL_WARNING) {
        logLevelStr = "WARNING";
    } else {
        if(logLevel != VICE_PLUGIN_API_LOG_LEVEL_INFO) {
            WARNING_LOG(
                "Incoming log message from vice plugin ", filename,
                "with unknown log level, defaulting to INFO"
            );
        }
        logLevelStr = "INFO";
    }

    LogWriter(
        logLevelStr,
        filename + " " + location
    )(msg);

API_CALLBACK_END
}

void panicCallback(void* filenamePtr, const char* location, const char* msg) {
API_CALLBACK_START

    const string& filename = *(string*)filenamePtr;
    Panicker(filename + " " + location)(msg);

API_CALLBACK_END
}

void destructorCallback(void* filenamePtr) {
API_CALLBACK_START

    string* filename = (string*)filenamePtr;
    delete filename;

API_CALLBACK_END
}

}

shared_ptr<VicePlugin> VicePlugin::load(string filename) {
    REQUIRE_UI_THREAD();

    void* lib = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if(lib == nullptr) {
        const char* err = dlerror();
        ERROR_LOG(
            "Loading vice plugin library '", filename,
            "' failed: ", err != nullptr ? err : "Unknown error"
        );
        return {};
    }

    unique_ptr<APIFuncs> apiFuncs = make_unique<APIFuncs>();

    void* sym;

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    sym = dlsym(lib, "vicePluginAPI_" #name); \
    if(sym == nullptr) { \
        const char* err = dlerror(); \
        ERROR_LOG( \
            "Loading symbol 'vicePluginAPI_" #name "' from vice plugin ", \
            filename, " failed: ", err != nullptr ? err : "Unknown error" \
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
            "Vice plugin ", filename,
            " does not support API version ", apiVersion
        );
        REQUIRE(dlclose(lib) == 0);
        return {};
    }

    apiFuncs->setLogCallback(
        apiVersion,
        logCallback,
        new string(filename),
        destructorCallback
    );
    apiFuncs->setPanicCallback(
        apiVersion,
        panicCallback,
        new string(filename),
        destructorCallback
    );

    return VicePlugin::create(
        CKey(),
        filename,
        lib,
        apiVersion,
        move(apiFuncs)
    );
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

string VicePlugin::getVersionString() {
    REQUIRE_UI_THREAD();

    char* raw = apiFuncs_->getVersionString();
    string val = raw;
    free(raw);
    return val;
}

vector<VicePlugin::OptionDocsItem> VicePlugin::getOptionDocs() {
    REQUIRE_UI_THREAD();

    vector<VicePlugin::OptionDocsItem> ret;

    apiFuncs_->getOptionDocs(
        apiVersion_,
        [](
            void* data,
            const char* name,
            const char* valSpec,
            const char* desc,
            const char* defaultValStr
        ) {
            vector<VicePlugin::OptionDocsItem>& ret =
                *(vector<VicePlugin::OptionDocsItem>*)data;
            ret.push_back({name, valSpec, desc, defaultValStr});
        },
        (void*)&ret
    );

    return ret;
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

    char* initErrorMsg = nullptr;
    VicePluginAPI_Context* handle = plugin->apiFuncs_->initContext(
        plugin->apiVersion_,
        optionNames.data(),
        optionValues.data(),
        options.size(),
        &initErrorMsg
    );

    if(handle == nullptr) {
        REQUIRE(initErrorMsg != nullptr);
        cerr
            << "ERROR: Vice plugin " << plugin->filename_ <<
            " initialization failed: " << initErrorMsg << "\n";
        free(initErrorMsg);
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
}

ViceContext::~ViceContext() {
    plugin_->apiFuncs_->destroyContext(handle_);
}
