#pragma once

// Vice (view service) API definition:
extern "C" {

// Opaque types:
typedef struct VicePlugin_Context VicePlugin_Context;

// API version independent functions:

// Returns true if the plugin supports given API version.
int vicePlugin_isAPIVersionSupported(uint64_t apiVersion);

// Functions for API version 1000000:

// Calls itemCallback immediately with the help information for each option
// supported by vicePlugin_initContext.
void vicePlugin_getOptionHelp(
    uint64_t apiVersion,
    void (*itemCallback)(
        void* data,
        const char* name,
        const char* valSpec,
        const char* desc,
        const char* defaultValStr
    ),
    void* itemCallbackData
);

// Initializes the plugin with given configuration options. Returns the
// created context on success, or NULL on failure. In case of failure,
// initErrorMsgCallback is called with the error message. The created
// context must be destroyed with vicePlugin_destroyContext.
VicePlugin_Context* vicePlugin_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    void (*initErrorMsgCallback)(void*, const char*),
    void* initErrorMsgCallbackData
);

}
