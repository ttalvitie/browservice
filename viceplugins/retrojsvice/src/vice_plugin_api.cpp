#include "common.hpp"

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

struct VicePluginAPI_Context {};

int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
API_FUNC_START

    return (int)(apiVersion == (uint64_t)1000000);

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

    INFO_LOG("Creating retrojsvice plugin context");

    for(size_t i = 0; i < optionCount; ++i) {
        REQUIRE(optionNames[i] != nullptr);
        REQUIRE(optionValues[i] != nullptr);

        string name = optionNames[i];
        string value = optionValues[i];

        if(
            name != "default-quality" &&
            name != "http-listen-addr" &&
            name != "http-max-threads" &&
            name != "http-auth"
        ) {
            setOutString(initErrorMsgOut, "Unknown option '" + name + "'");
            return nullptr;
        }
        if(value == "") {
            setOutString(initErrorMsgOut, "Invalid value '" + value + "' for option '" + name + "'");
            return nullptr;
        }
    }

    return new VicePluginAPI_Context;

API_FUNC_END
}

void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx) {
API_FUNC_START

    REQUIRE(ctx != nullptr);

    INFO_LOG("Destroying retrojsvice plugin context");

    delete ctx;

API_FUNC_END
}

void vicePluginAPI_setLogCallback(
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

void vicePluginAPI_setPanicCallback(
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
