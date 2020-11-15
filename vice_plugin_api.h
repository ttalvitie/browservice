#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************************
 *** Vice Plugin API Definition ***
 **********************************/

/* General API guidelines:
 *   - The user must use vicePluginAPI_isAPIVersionSupported to find an API version that both the user
 *       and the plugin support. In other plugin API functions, setting the apiVersion argument to
 *       an unsupported version or using API functions that are not part of the chosen API version
 *       results in undefined behavior.
 *   - Every callback argument to an API function is followed by a data argument of type void*. The
 *       plugin must pass this data argument as the first argument when calling the callback.
 *   - The implementation of a plugin API function or a callback functions passed to a plugin API
 *       function must not retain pointers to their arguments after the function has returned,
 *       unless specifically allowed by the documentation.
 *   - The user must synchronize its calls to the API functions such that two calls concerning the
 *       same VicePluginAPI_Context are never running concurrently.
 *   - The plugin may call callbacks given to it in ANY thread. It may also call multiple callback
 *       functions concurrently.
 *   - The API is pure C; API functions and callbacks should not pass C++ exceptions to the caller.
 */

/********************
 *** Opaque types ***
 ********************/

typedef struct VicePluginAPI_Context VicePluginAPI_Context;

/********************************************
 *** Functions common to all API versions ***
 ********************************************/

/* Returns 1 if the plugin supports the given API version; otherwise, returns 0. */
int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion);

/*****************************************
 *** Functions for API version 1000000 ***
 *****************************************/

/* For each configuration option supported for vicePluginAPI_initContext, calls itemCallback in the
 * current thread with the documentation for that option:
 *   - name: The name of the option. Convention: lower case, words separated by dashes.
 *   - valSpec: Short description of the value. Convention: upper case, no spaces.
 *   - desc: Textual description. Convention: no capitalization in first word.
 *   - defaultValStr: Short description of what happens if the option is omitted (for example).
 *       Convention: Start with "default".
 * Example itemCallback call:
 *   itemCallback(
 *     itemCallbackData,
 *     "http-listen-addr",
 *     "IP:PORT",
 *     "bind address and port for the HTTP server",
 *     "default: 127.0.0.1:8080"
 *   );
 */
void vicePluginAPI_getOptionHelp(
    uint64_t apiVersion,
    void (*itemCallback)(
        void*,
        const char* name,
        const char* valSpec,
        const char* desc,
        const char* defaultValStr
    ),
    void* itemCallbackData
);

/* Initializes the plugin with given configuration options and utility callbacks, returning the
 * created context on success. In case of failure, NULL is returned and initErrorMsgCallback is
 * called with the error message in the current thread. If creating the context is successful, the
 * other callbacks, panicCallback and logCallback, are retained in the returned context and may be
 * called by the context at any point before the context is destroyed. Both panicCallback and
 * logCallback should attempt to show the given message to the user. A call to panicCallback must
 * not return; instead, it must end the process immediately (for example by calling abort()).
 * Arguments for the callbacks:
 *   - msg: Textual description of the event/error.
 *   - location: The source of the error, such as "vice.cpp:132".
 *   - severity: The importance of the log message: 0 for INFO, 1 for WARNING and 2 for ERROR.
 */
VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    void (*initErrorMsgCallback)(void*, const char* msg),
    void* initErrorMsgCallbackData,
    void (*panicCallback)(void*, const char* location, const char* msg),
    void* panicCallbackData,
    void (*logCallback)(void*, int severity, const char* location, const char* msg),
    void* logCallbackData
);

/* Destroy given vice plugin context previously initialized by vicePluginAPI_initContext. */
void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx);

#ifdef __cplusplus
}
#endif
