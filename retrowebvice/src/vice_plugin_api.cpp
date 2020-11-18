#include "context.hpp"

#include "../../vice_plugin_api.h"

using namespace retrowebvice;

struct VicePluginAPI_Context {
    void (*panicCallback)(void*, const char*, const char*);
    void* panicCallbackData;
    Context* impl;
};

#define ABORT_ON_EXCEPTION_IMPL(what) \
    cerr \
        << "FATAL @ vice plugin " << __FILE__ << ":" << __LINE__  \
        << ": Unhandled exception traversing vice plugin API: " << (what) << "\n"; \
    cerr.flush(); \
    abort();

#define ABORT_ON_EXCEPTION \
    catch(const exception& e) { \
        ABORT_ON_EXCEPTION_IMPL(e.what()) \
    } catch(...) { \
        ABORT_ON_EXCEPTION_IMPL("Unknown exception") \
    }

#define PANIC_ON_EXCEPTION_IMPL(callbackPath, what) \
    stringstream msgss; \
    msgss << "Unhandled exception traversing vice plugin API: " << (what); \
    string msg = msgss.str(); \
    callbackPath panicCallback( \
        callbackPath panicCallbackData, \
        __FILE__ ":" STRINGIFY(__LINE__), \
        msg.c_str() \
    ); \
    abort();

#define PANIC_ON_EXCEPTION(callbackPath) \
    catch(const exception& e) { \
        PANIC_ON_EXCEPTION_IMPL(callbackPath, e.what()) \
    } catch(...) { \
        PANIC_ON_EXCEPTION_IMPL(callbackPath, "Unknown exception") \
    }

int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
    try {
        return (int)(apiVersion == (uint64_t)1000000);
    } ABORT_ON_EXCEPTION
}

void vicePluginAPI_getOptionHelp(
    uint64_t apiVersion,
    void (*itemCallback)(void*, const char*, const char*, const char*, const char*),
    void* itemCallbackData
) {
    try {
        if(apiVersion == (uint64_t)1000000) {
            vector<tuple<string, string, string, string>> docs =
                Context::supportedOptionDocs();

            for(const tuple<string, string, string, string>& doc : docs) {
                string name, valSpec, desc, defaultValStr;
                tie(name, valSpec, desc, defaultValStr) = doc;
                itemCallback(
                    itemCallbackData,
                    name.c_str(),
                    valSpec.c_str(),
                    desc.c_str(),
                    defaultValStr.c_str()
                );
            }
        }
    } ABORT_ON_EXCEPTION
}

VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    void (*initErrorMsgCallback)(void*, const char*),
    void* initErrorMsgCallbackData,
    void (*panicCallback)(void*, const char*, const char*),
    void* panicCallbackData,
    void (*logCallback)(void*, int, const char*, const char*),
    void* logCallbackData
) {
    try {
        if(apiVersion != (uint64_t)1000000) {
            initErrorMsgCallback(initErrorMsgCallbackData, "Unsupported API version");
            return nullptr;
        }

        vector<pair<string, string>> options;
        for(size_t i = 0; i < optionCount; ++i) {
            options.emplace_back(optionNames[i], optionValues[i]);
        }

        variant<unique_ptr<Context>, string> result = Context::create(
            options,
            [panicCallback, panicCallbackData](string location, string msg) {
                panicCallback(panicCallbackData, location.c_str(), msg.c_str());
            },
            [logCallback, logCallbackData](string location, string msg) {
                logCallback(logCallbackData, 0, location.c_str(), msg.c_str());
            },
            [logCallback, logCallbackData](string location, string msg) {
                logCallback(logCallbackData, 1, location.c_str(), msg.c_str());
            },
            [logCallback, logCallbackData](string location, string msg) {
                logCallback(logCallbackData, 2, location.c_str(), msg.c_str());
            }
        );

        unique_ptr<Context> impl;
        if(unique_ptr<Context>* implPtr = get_if<unique_ptr<Context>>(&result)) {
            impl = move(*implPtr);
        } else {
            string msg = get<string>(result);
            initErrorMsgCallback(initErrorMsgCallbackData, msg.c_str());
            return nullptr;
        }

        VicePluginAPI_Context* ctx = new VicePluginAPI_Context;
        ctx->panicCallback = panicCallback;
        ctx->panicCallbackData = panicCallbackData;
        ctx->impl = impl.release();

        return ctx;
    } PANIC_ON_EXCEPTION()
}

#define CHECK_NULL_CTX() \
    do { \
        if(ctx == nullptr || ctx->impl == nullptr) { \
            cerr \
                << "FATAL @ " << __FUNCTION__  \
                << ": Plugin API function called with NULL context\n"; \
            cerr.flush(); \
            abort(); \
        } \
    } while(false)

void vicePluginAPI_startContext(VicePluginAPI_Context* ctx) {
    CHECK_NULL_CTX();
    try {
        ctx->impl->start();
    } PANIC_ON_EXCEPTION(ctx->)
}

void vicePluginAPI_asyncStopContext(
    VicePluginAPI_Context* ctx,
    void (*stopCompleteCallback)(void*),
    void* stopCompleteCallbackData
) {
    CHECK_NULL_CTX();
    try {
        ctx->impl->asyncStop(
            [stopCompleteCallback, stopCompleteCallbackData]() {
                stopCompleteCallback(stopCompleteCallbackData);
            }
        );
    } PANIC_ON_EXCEPTION(ctx->)
}

void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx) {
    CHECK_NULL_CTX();

    auto panicCallback = ctx->panicCallback;
    void* panicCallbackData = ctx->panicCallbackData;

    try {
        delete ctx->impl;
        ctx->impl = nullptr;
        delete ctx;
    } PANIC_ON_EXCEPTION()
}
