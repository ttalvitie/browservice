#include "vice_plugin_api.hpp"

#include "context.hpp"
#include "credits.hpp"

namespace {

using namespace retrojsvice;

const char* RetrojsviceVersion = "0.9.6.1";

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

#ifdef _WIN32
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT __attribute__((visibility("default")))
#endif

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
    uint64_t apiVersion;
    shared_ptr<Context> impl;
};

API_EXPORT int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
API_FUNC_START

    return (int)(apiVersion == (uint64_t)2000000);

API_FUNC_END
}

API_EXPORT char* vicePluginAPI_createVersionString() {
API_FUNC_START

    return createMallocString(string("Retrojsvice ") + RetrojsviceVersion);

API_FUNC_END
}

API_EXPORT char* vicePluginAPI_createCreditsString() {
API_FUNC_START

    return createMallocString(credits);

API_FUNC_END
}

// Must be a wrapper for malloc() as the rest of the plugin implementation uses free() directly
API_EXPORT void* vicePluginAPI_malloc(size_t size) {
API_FUNC_START

    return malloc(size);

API_FUNC_END
}

// Must be a wrapper for free() as the rest of the plugin implementation uses malloc() directly
API_EXPORT void vicePluginAPI_free(void* ptr) {
API_FUNC_START

    return free(ptr);

API_FUNC_END
}

API_EXPORT VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    const char* programName,
    char** initErrorMsgOut
) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)2000000);

    REQUIRE(programName != nullptr);

    vector<pair<string, string>> options;
    for(size_t i = 0; i < optionCount; ++i) {
        REQUIRE(optionNames != nullptr);
        REQUIRE(optionValues != nullptr);
        REQUIRE(optionNames[i] != nullptr);
        REQUIRE(optionValues[i] != nullptr);

        options.emplace_back(optionNames[i], optionValues[i]);
    }

    variant<shared_ptr<Context>, string> result =
        Context::init(options, programName);

    shared_ptr<Context> impl;
    if(shared_ptr<Context>* implPtr = get_if<shared_ptr<Context>>(&result)) {
        impl = *implPtr;
    } else {
        string msg = get<string>(result);
        setOutString(initErrorMsgOut, msg);
        return nullptr;
    }

    VicePluginAPI_Context* ctx = new VicePluginAPI_Context;
    ctx->apiVersion = apiVersion;
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

// Convenience macros for creating implementations of API functions that forward
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

API_EXPORT int vicePluginAPI_createPopupWindow(
    VicePluginAPI_Context* ctx,
    uint64_t parentWindow,
    uint64_t popupWindow,
    char** msg
)
WRAP_CTX_API(createPopupWindow, parentWindow, popupWindow, msg)

API_EXPORT void vicePluginAPI_closeWindow(
    VicePluginAPI_Context* ctx,
    uint64_t window
)
WRAP_CTX_API(closeWindow, window)

API_EXPORT void vicePluginAPI_notifyWindowViewChanged(
    VicePluginAPI_Context* ctx,
    uint64_t window
)
WRAP_CTX_API(notifyWindowViewChanged, window)

API_EXPORT void vicePluginAPI_setWindowCursor(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    VicePluginAPI_MouseCursor cursor
)
WRAP_CTX_API(setWindowCursor, window, cursor)

API_EXPORT int vicePluginAPI_windowQualitySelectorQuery(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    char** qualityListOut,
    size_t* currentQualityOut
)
WRAP_CTX_API(
    windowQualitySelectorQuery, window, qualityListOut, currentQualityOut
)

API_EXPORT void vicePluginAPI_windowQualityChanged(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    size_t qualityIdx
)
WRAP_CTX_API(windowQualityChanged, window, qualityIdx)

API_EXPORT int vicePluginAPI_windowNeedsClipboardButtonQuery(
    VicePluginAPI_Context* ctx,
    uint64_t window
)
WRAP_CTX_API(windowNeedsClipboardButtonQuery, window)

API_EXPORT void vicePluginAPI_windowClipboardButtonPressed(
    VicePluginAPI_Context* ctx,
    uint64_t window
)
WRAP_CTX_API(windowClipboardButtonPressed, window)

API_EXPORT void vicePluginAPI_putClipboardContent(
    VicePluginAPI_Context* ctx,
    const char* text
)
WRAP_CTX_API(putClipboardContent, text)

API_EXPORT void vicePluginAPI_putFileDownload(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    const char* name,
    const char* path,
    void (*cleanup)(void*),
    void* cleanupData
)
WRAP_CTX_API(putFileDownload, window, name, path, cleanup, cleanupData)

API_EXPORT int vicePluginAPI_startFileUpload(
    VicePluginAPI_Context* ctx,
    uint64_t window
)
WRAP_CTX_API(startFileUpload, window)

API_EXPORT void vicePluginAPI_cancelFileUpload(
    VicePluginAPI_Context* ctx,
    uint64_t window
)
WRAP_CTX_API(cancelFileUpload, window)

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

    REQUIRE(apiVersion == (uint64_t)2000000);
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
    void (*callback)(
        void* data,
        VicePluginAPI_LogLevel logLevel,
        const char* location,
        const char* msg
    ),
    void* data,
    void (*destructorCallback)(void* data)
) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)2000000);

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
                VicePluginAPI_LogLevel apiLogLevel;
                if(logLevel == LogLevel::Error) {
                    apiLogLevel = VICE_PLUGIN_API_LOG_LEVEL_ERROR;
                } else if(logLevel == LogLevel::Warning) {
                    apiLogLevel = VICE_PLUGIN_API_LOG_LEVEL_WARNING;
                } else {
                    REQUIRE(logLevel == LogLevel::Info);
                    apiLogLevel = VICE_PLUGIN_API_LOG_LEVEL_INFO;
                }
                func(apiLogLevel, location, msg);
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

    REQUIRE(apiVersion == (uint64_t)2000000);

    if(callback == nullptr) {
        setPanicCallback({});
    } else {
        setPanicCallback(
            GlobalCallback<decltype(callback)>(callback, data, destructorCallback)
        );
    }

API_FUNC_END
}

API_EXPORT int vicePluginAPI_isExtensionSupported(uint64_t apiVersion, const char* name) {
API_FUNC_START

    REQUIRE(apiVersion == (uint64_t)2000000);

    string nameStr = name;
    if(nameStr == "URINavigation") {
        return 1;
    } else {
        return 0;
    }

API_FUNC_END
}

API_EXPORT void vicePluginAPI_URINavigation_enable(
    VicePluginAPI_Context* ctx,
    VicePluginAPI_URINavigation_Callbacks callbacks
)
WRAP_CTX_API(URINavigation_enable, callbacks);

}
