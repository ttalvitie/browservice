#include "common.hpp"

#include "../../vice_plugin_api.hpp"

int vicePlugin_isAPIVersionSupported(uint64_t apiVersion) {
    return (int)(apiVersion == (uint64_t)1000000);
}

void vicePlugin_getOptionHelp(
    uint64_t apiVersion,
    void (*itemCallback)(void*, const char*, const char*, const char*, const char*),
    void* itemCallbackData
) {
    if(apiVersion == (uint64_t)1000000) {
        itemCallback(
            itemCallbackData,
            "default-quality",
            "QUALITY",
            "initial image quality for each session (10..100 or PNG)",
            "default: PNG"
        );
        itemCallback(
            itemCallbackData,
            "http-listen-addr",
            "IP:PORT",
            "bind address and port for the HTTP server",
            "default: 127.0.0.1:8080"
        );
        itemCallback(
            itemCallbackData,
            "http-auth",
            "USER:PASSWORD",
            "if nonempty, the client is required to authenticate using "
            "HTTP basic authentication with given username and "
            "password; if the special value 'env' is specified, the "
            "value is read from the environment variable "
            "BROWSERVICE_HTTP_AUTH_CREDENTIALS",
            "default empty"
        );
    }
}

struct VicePlugin_Context {};

VicePlugin_Context* vicePlugin_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    void (*initErrorCallback)(void*, const char*),
    void* initErrorCallbackData
) {
    if(apiVersion != (uint64_t)1000000) {
        initErrorCallback(initErrorCallbackData, "Unsupported API version");
        return nullptr;
    }

    for(size_t i = 0; i < optionCount; ++i) {
        string name = optionNames[i];
        string value = optionValues[i];
        if(name != "default-quality" && name != "http-auth" && name != "http-listen-addr") {
            string msg = "Unrecognized option '" + name + "'";
            initErrorCallback(initErrorCallbackData, msg.c_str());
            return nullptr;
        }
    }

    return new VicePlugin_Context();
}
