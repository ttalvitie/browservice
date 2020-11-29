#include "context.hpp"

#include "../../../vice_plugin_api.h"

namespace {

using namespace retrojsvice;

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

char* createMallocString(string val) {
    size_t size = val.size() + 1;
    char* ret = (char*)malloc(size);
    REQUIRE(ret != nullptr);
    memcpy(ret, val.c_str(), size);
    return ret;
}

void setOutString(char** out, string val) {
    if(out != nullptr) {
        *out = createMallocString(val);
    }
}

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

int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
API_FUNC_START

    return (int)(apiVersion == (uint64_t)1000000);

API_FUNC_END
}

char* vicePluginAPI_getVersionString() {
API_FUNC_START

    return createMallocString("Retrojsvice 0.9.1.2");

API_FUNC_END
}

VicePluginAPI_Context* vicePluginAPI_initContext(
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

void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx) {
API_FUNC_START

    REQUIRE(ctx != nullptr);
    delete ctx;

API_FUNC_END
}

void vicePluginAPI_start(
    VicePluginAPI_Context* ctx,
    void (*eventNotifyCallback)(void* data),
    void* eventNotifyData,
    void (*shutdownCompleteCallback)(void* data),
    void* shutdownCompleteData
) {
API_FUNC_START

    REQUIRE(ctx != nullptr);

    ctx->impl->start(
        [eventNotifyCallback, eventNotifyData]() {
            eventNotifyCallback(eventNotifyData);
        },
        [shutdownCompleteCallback, shutdownCompleteData]() {
            shutdownCompleteCallback(shutdownCompleteData);
        }
    );

API_FUNC_END
}

void vicePluginAPI_shutdown(VicePluginAPI_Context* ctx) {
API_FUNC_START

    REQUIRE(ctx != nullptr);
    ctx->impl->shutdown();

API_FUNC_END
}

void vicePluginAPI_pumpEvents(VicePluginAPI_Context* ctx) {
API_FUNC_START

    REQUIRE(ctx != nullptr);
    ctx->impl->pumpEvents();

API_FUNC_END
}

void vicePluginAPI_getOptionDocs(
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

    callback(
        data,
        "default-quality",
        "QUALITY",
        "initial image quality for each session (10..100 or PNG)",
        "default: PNG"
    );
    callback(
        data,
        "http-listen-addr",
        "IP:PORT",
        "bind address and port for the HTTP server",
        "default: 127.0.0.1:8080"
    );
    callback(
        data,
        "http-max-threads",
        "COUNT",
        "maximum number of HTTP server threads",
        "default: 100"
    );
    callback(
        data,
        "http-auth",
        "USER:PASSWORD",
        "if nonempty, the client is required to authenticate using "
        "HTTP basic authentication with given username and "
        "password; if the special value 'env' is specified, the "
        "value is read from the environment variable "
        "HTTP_AUTH_CREDENTIALS",
        "default empty"
    );

API_FUNC_END
}

void vicePluginAPI_setGlobalLogCallback(
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

void vicePluginAPI_setGlobalPanicCallback(
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
