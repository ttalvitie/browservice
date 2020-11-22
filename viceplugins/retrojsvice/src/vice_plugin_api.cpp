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

#define API_FUNC_START try {
#define API_FUNC_END \
    } catch(const exception& e) { \
        PANIC("Unhandled exception traversing the vice plugin API: ", e.what()); \
    } catch(...) { \
        PANIC("Unhandled exception traversing the vice plugin API"); \
    }

}

extern "C" {

int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
API_FUNC_START

    return (int)(apiVersion == (uint64_t)1000000);

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
