#include "context.hpp"

#include "../../../vice_plugin_api.h"

namespace {

using namespace retrojsvice;

const char* RetrojsviceVersion = "0.9.1.2";

template <typename T>
class GlobalCallback {
private:
    struct Inner {
        T callback;
        void* data;
        void (*destructorCallback)(void* data);

        Inner(
            T callback,
            void* data,
            void (*destructorCallback)(void* data)
        ) :
            callback(callback),
            data(data),
            destructorCallback(destructorCallback)
        {}

        DISABLE_COPY_MOVE(Inner);

        ~Inner() {
            if(destructorCallback != nullptr) {
                destructorCallback(data);
            }
        }
    };
    shared_ptr<Inner> inner_;

public:
    template <typename... Arg>
    GlobalCallback(Arg... arg) : inner_(make_shared<Inner>(arg...)) {}

    template <typename... Arg>
    void operator()(Arg... arg) const {
        inner_->callback(inner_->data, arg...);
    }
};

void setOutString(char** out, string val) {
    if(out != nullptr) {
        *out = createMallocString(val);
    }
}

#define API_EXPORT __attribute__((visibility("default")))

#define API_FUNC_START \
    try {
#define API_FUNC_END \
    } catch(const exception& e) { \
        PANIC("Unhandled exception traversing the vice plugin API: ", e.what()); \
        abort(); \
    } catch(...) { \
        PANIC("Unhandled exception traversing the vice plugin API"); \
        abort(); \
    }

}

extern "C" {

struct VicePluginAPI_Context {
    shared_ptr<Context> impl;
};

API_EXPORT int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
API_FUNC_START

    return (int)(apiVersion == (uint64_t)1000000);

API_FUNC_END
}

API_EXPORT char* vicePluginAPI_getVersionString() {
API_FUNC_START

    return createMallocString(string("Retrojsvice ") + RetrojsviceVersion);

API_FUNC_END
}

API_EXPORT VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    char** initErrorMsgOut
) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)1000000);

    vector<pair<string, string>> options;
    for(size_t i = 0; i < optionCount; ++i) {
        REQUIRE(optionNames[i] != nullptr);
        REQUIRE(optionValues[i] != nullptr);

        options.emplace_back(optionNames[i], optionValues[i]);
    }

    variant<shared_ptr<Context>, string> result = Context::init(options);

    shared_ptr<Context> impl;
    if(shared_ptr<Context>* implPtr = get_if<shared_ptr<Context>>(&result)) {
        impl = *implPtr;
    } else {
        string msg = get<string>(result);
        setOutString(initErrorMsgOut, msg);
        return nullptr;
    }

    VicePluginAPI_Context* ctx = new VicePluginAPI_Context;
    ctx->impl = impl;
    return ctx;

API_FUNC_END
}

API_EXPORT void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx) {
API_FUNC_START

    REQUIRE(ctx != nullptr);
    delete ctx;

API_FUNC_END
}

// Convenience macro for creating implementations of API functions that forward
// their arguments to corresponding member functions of the Context
#define WRAP_CTX_API(funcName, ...) \
    { \
    API_FUNC_START \
        REQUIRE(ctx != nullptr); \
        return ctx->impl->funcName(__VA_ARGS__); \
    API_FUNC_END \
    }

API_EXPORT void vicePluginAPI_start(
    VicePluginAPI_Context* ctx,
    VicePluginAPI_Callbacks callbacks,
    void* callbackData
)
WRAP_CTX_API(start, callbacks, callbackData)

API_EXPORT void vicePluginAPI_shutdown(VicePluginAPI_Context* ctx)
WRAP_CTX_API(shutdown)

API_EXPORT void vicePluginAPI_pumpEvents(VicePluginAPI_Context* ctx)
WRAP_CTX_API(pumpEvents)

API_EXPORT void vicePluginAPI_notifyWindowViewChanged(
    VicePluginAPI_Context* ctx,
    uint64_t handle
)
WRAP_CTX_API(notifyWindowViewChanged, handle)

API_EXPORT void vicePluginAPI_getOptionDocs(
    uint64_t apiVersion,
    void (*callback)(
        void* data,
        const char* name,
        const char* valSpec,
        const char* desc,
        const char* defaultValStr
    ),
    void* data
) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)1000000);
    REQUIRE(callback != nullptr);

    vector<tuple<string, string, string, string>> docs =
        Context::getOptionDocs();

    for(const tuple<string, string, string, string>& doc : docs) {
        string name, valSpec, desc, defaultValStr;
        tie(name, valSpec, desc, defaultValStr) = doc;
        callback(
            data,
            name.c_str(),
            valSpec.c_str(),
            desc.c_str(),
            defaultValStr.c_str()
        );
    }

API_FUNC_END
}

API_EXPORT void vicePluginAPI_setGlobalLogCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, int logLevel, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)1000000);

    if(callback == nullptr) {
        setLogCallback({});
    } else {
        GlobalCallback<decltype(callback)> func(
            callback, data, destructorCallback
        );
        setLogCallback(
            [func](
                LogLevel logLevel,
                const char* location,
                const char* msg
            ) {
                int logLevelID;
                if(logLevel == LogLevel::Error) {
                    logLevelID = VICE_PLUGIN_API_LOG_LEVEL_ERROR;
                } else if(logLevel == LogLevel::Warning) {
                    logLevelID = VICE_PLUGIN_API_LOG_LEVEL_WARNING;
                } else {
                    REQUIRE(logLevel == LogLevel::Info);
                    logLevelID = VICE_PLUGIN_API_LOG_LEVEL_INFO;
                }
                func(logLevelID, location, msg);
            }
        );
    }

API_FUNC_END
}

API_EXPORT void vicePluginAPI_setGlobalPanicCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)1000000);

    if(callback == nullptr) {
        setPanicCallback({});
    } else {
        setPanicCallback(
            GlobalCallback<decltype(callback)>(callback, data, destructorCallback)
        );
    }

API_FUNC_END
}

}
