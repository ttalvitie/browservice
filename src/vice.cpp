#include "vice.hpp"

#include "../vice_plugin_api.h"

#include <dlfcn.h>

struct VicePlugin::APIFuncs {
#define FOREACH_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isAPIVersionSupported)

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    decltype(&vicePluginAPI_ ## name) name;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM
};

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
