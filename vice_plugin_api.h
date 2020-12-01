#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
 *** Vice Plugin API Definition ***
 **********************************/

/* This file describes the common API for vice plugins: shared libraries that can be used by an
 * interactive program to show its GUI to the user.
 *
 * The vice plugin API was originally designed as an abstraction of the user-facing part of the
 * Browservice web "proxy" server. Browservice makes it possible to browse the modern web on
 * obsolete operating systems and hardware by rendering the browser UI on the server side. The
 * plugin architecture makes it easy to add support for different types of clients to Browservice
 * while reusing most of the code.
 *
 * The API is not specific to Browservice, and thus the same plugins may be used for other kinds of
 * GUI programs as well.
 *
 * The GUI shown by the plugin consists of multiple windows; the program supplies the plugin with
 * updates to a resizable 24-bit RGB image view for each window, and the plugin sends the keyboard
 * and mouse events concerning the windows back to the program. In addition, passing clipboard text
 * and file downloads through the plugin is supported by the API.
 *
 * Typical API usage for API version 1000000:
 *
 *  1. The program verifies that the plugin supports API version 1000000 by calling
 *     vicePluginAPI_isAPIVersionSupported.
 *
 *  2. (optional) The program registers its logging and panicking functions to the plugin using
 *     vicePluginAPI_setGlobalLogCallback and vicePluginAPI_setGlobalPanicCallback, allowing the
 *     plugin to use them instead of its default logging and panicking behavior.
 *
 *  3. The program initializes a plugin context using vicePluginAPI_initContext, supplying it with
 *     configuration options (name-value-pairs). If the plugin is selectable by the user, then these
 *     configuration options should also be specified by the user, because the options are
 *     plugin-specific. The function may also return an error (for example if the configuration
 *     options are invalid); the program should show this error to the user. The program may query
 *     for the documentation of the options by calling vicePluginAPI_getOptionDocs.
 *
 *  4. The program registers callbacks for the plugin context to use after it has been started (see
 *     below) using the vicePluginAPI_set*Callback(s) functions. The program should call at least
 *     vicePluginAPI_setWindowCallbacks to enable window management (otherwise the plugin does
 *     nothing) (TODO).
 *
 *  5. The program starts the operation of the plugin context by calling vicePluginAPI_start.
 *     After this, the program and plugin communicate as follows:
 *
 *       - The program may call API functions directly; however, it must never make two API function
 *         calls concerning the same context concurrently.
 *
 *       - To avoid concurrency issues, the plugin context may not call callbacks given to it
 *         directly from its background threads. Instead, it is synchronized to the program event
 *         loop as follows: The plugin notifies the program that it has events to process by calling
 *         the event notification callback (given in vicePluginAPI_start) in any thread at any time.
 *         After a notification like this, the program should call vicePlugin_pumpEvents. The
 *         implementation of vicePlugin_pumpEvents may then call the callbacks directly.
 *
 *  6. To start shutting down the plugin context, the program should call vicePluginAPI_shutdown.
 *     When the plugin has shut down, it will respond by calling the shutdown complete callback
 *     provided in vicePluginAPI_start (like all callbacks calls, this will happen in
 *     vicePlugin_pumpEvents).
 *
 *  7. The program destroys the plugin context using vicePluginAPI_destroyContext.
 *
 * General API conventions:
 *
 *   - No function in the API may retain and use pointers given to it as arguments after the
 *     function has returned, unless specifically allowed by the documentation. This applies to
 *     functions in the plugin API as well as callback functions given to the plugin through the
 *     API.
 *
 *   - Each callback argument in the API is followed by a void* data argument, which is always
 *     passed as the first argument when calling the callback.
 *
 *   - To avoid reentrancy issues,
 *
 *       * implementations of the plugin API functions may not directly call callbacks supplied by
 *         the program, and
 *
 *       * implementations of the callbacks may not directly call plugin API functions
 *
 *     unless specifically allowed by the documentation (as is the case for example in
 *     vicePluginAPI_pumpEvents). To get around this restriction, a task queue should be used to
 *     defer the calls.
 *
 *   - Most of the API supports only a very simple form of error handling on the plugin side: either
 *     recovering from the error (and optionally logging a warning) or panicking and terminating the
 *     program.
 *
 *   - This API is not thread safe for concurrent calls concerning the same plugin context. However,
 *     calls concerning different contexts and calls not related to plugin contexts may be made from
 *     different threads concurrently.
 *
 *   - This API is asynchronous/non-blocking; API functions and callbacks should not block for I/O.
 *     All blocking should be done in background threads. As the API for a single context is
 *     synchronous, CPU-intensive operations (such as image compression) should also be offloaded to
 *     background threads to avoid stalling the whole program.
 *
 *   - This API is pure C; API functions and callbacks must not let C++ exceptions fall through to
 *     the caller.
 */

/***************************************************************************************************
 *** Functions common to all API versions ***
 ********************************************/

/* Returns 1 if the plugin supports the given API version; otherwise, returns 0. */
int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion);

/* Returns a string describing the name and version of the plugin. The caller is responsible for
 * freeing the string using free().
 */
char* vicePluginAPI_getVersionString();

/*************
 * Constants *
 *************/

#define VICE_PLUGIN_API_LOG_LEVEL_INFO 0
#define VICE_PLUGIN_API_LOG_LEVEL_WARNING 10
#define VICE_PLUGIN_API_LOG_LEVEL_ERROR 20

/*********
 * Types *
 *********/

/* Opaque type for plugin contexts. */
typedef struct VicePluginAPI_Context VicePluginAPI_Context;

/***************************************************************************************************
 *** Functions for API version 1000000 ***
 *****************************************/

/**************************************
 * General context handling functions *
 **************************************/

/* Initializes a new plugin context with configuration options given as name-value-pairs
 * (optionNames[i], optionValues[i]) for 0 <= i < optionCount, returning the created context on
 * success. (Documentation of the supported configuration options can be queried using
 * vicePluginAPI_getOptionDocs.)
 *
 * In case of failure, NULL is returned and if initErrorMsg is not NULL, *initErrorMsg is set to
 * point to a string describing the reason for the failure; the caller must free the string using
 * free().
 *
 * The program may attempt create multiple independent contexts for the same plugin; if the plugin
 * does not support this and the program attempts to create a second context, this function should
 * fail with a descriptive error message.
 */
VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    char** initErrorMsgOut
);

/* Destroy a vice plugin context that was previously initialized by vicePluginAPI_initContext. If
 * the context has been started using vicePluginAPI_start, it must have been successfully shut down
 * (by calling vicePluginAPI_shutdown and waiting for the shutdownCompleteCallback given in
 * vicePluginAPI_start to be called).
 */
void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx);

/* Start running the actually running given plugin context. This function may be called only once
 * per context.
 *
 * A running plugin may call eventNotifyCallback from any thread at any time (even from an API
 * function invoked by the program). When receiving such call, the program must ensure that
 * vicePluginAPI_pumpEvents is called as soon as possible; this will allow the plugin to call
 * callbacks that have been registered to it using vicePluginAPI_set*Callback(s). Note that the
 * program may not call vicePluginAPI_pumpEvents or any other API function concerning this context
 * from the callback itself.
 *
 * In addition to starting the context, this function will register shutdownCompleteCallback to the
 * context. It will be called once from vicePluginAPI_pumpEvents when the plugin shutdown (initiated
 * by the program using vicePluginAPI_shutdown) is complete; after this, the context is no longer
 * running. This means that it will not call any further callbacks and the program is not allowed
 * call any other API functions for the context except for vicePluginAPI_destroyContext.
 */
void vicePluginAPI_start(
    VicePluginAPI_Context* ctx,
    void (*eventNotifyCallback)(void* data),
    void* eventNotifyData,
    void (*shutdownCompleteCallback)(void* data),
    void* shutdownCompleteData
);

/* Request shutdown of a running plugin context. This function may be called only once per context.
 * When the shutdown is complete, the plugin will call shutdownCompleteCallback (registered in
 * vicePluginAPI_start); after this, the program must destroy the context using
 * vicePluginAPI_destroyContext and refrain from calling any other API functions for this context.
 */
void vicePluginAPI_shutdown(VicePluginAPI_Context* ctx);

/* Allows a running plugin context to make progress in its own task queue. May call callbacks
 * registered to the context directly in the current thread before returning.
 *
 * This function should be called by the program if eventNotifyCallback (registered in
 * vicePluginAPI_start) has been called after this function was invoked the last time. Note that
 * eventNotifyCallback may be called while this function is running; in that case, this function
 * should be called again. The program may call this function even when eventNotifyCallback has not
 * been called. It is sufficient to call this function only once even if eventNotifyCallback has
 * been called multiple times after the previous call.
 */
void vicePluginAPI_pumpEvents(VicePluginAPI_Context* ctx);

/**********************************
 * Non-context-specific functions *
 **********************************/

/* Supplies the documentation for the configuration options supported by vicePluginAPI_initContext
 * by repeatedly calling given callback in the current thread before returning. Each call gives the
 * documentation for a single configuration option in its arguments:
 *   - name: The name of the option. Convention: lower case, words separated by dashes.
 *   - valSpec: Short description of the value. Convention: upper case, no spaces.
 *   - desc: Textual description. Convention: no capitalization of the first letter.
 *   - defaultValStr: Short description of what happens if the option is omitted. Convention: Start
 *       with "default".
 *
 * Example callback call:
 *   callback(
 *     data,
 *     "http-listen-addr",
 *     "IP:PORT",
 *     "bind address and port for the HTTP server",
 *     "default: 127.0.0.1:8080"
 *   );
 */
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
);

/* The following two functions may be called to allow the plugin to use given callback for logging
 * or panicking instead of the default behavior. After this, the plugin may call the given callback
 * from any thread at any time.
 *
 * The logging callback should log the given message with the appropriate context information for
 * the user to see. The panicking callback must never return and it must end the process in a timely
 * manner (for example using abort()).
 *
 * Arguments for callback:
 *   - logLevel: The severity of the log event. Allowed values: VICE_PLUGIN_API_LOG_LEVEL_INFO,
 *       VICE_PLUGIN_API_LOG_LEVEL_WARNING and VICE_PLUGIN_API_LOG_LEVEL_ERROR.
 *   - location: String describing the source of the event. Example: "viceplugin.cpp:142".
 *   - msg: Message string.
 *
 * Passing NULL as the callback reverts the plugin back to the default behavior for logging or
 * panicking. In this case, the data and destructorCallback arguments are ignored.
 *
 * If destructorCallback is not NULL, the plugin should call it with the given data argument if
 * it knows it will no longer use the callback (for example when the plugin is unloaded or the
 * callback is reset with another call to vicePluginAPI_set*Callback).
 *
 * The plugin may also choose to ignore the given callback completely. One valid implementation
 * for both of the functions is:
 * { if(destructorCallback) destructorCallback(data); }
 */
void vicePluginAPI_setGlobalLogCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, int logLevel, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
);
void vicePluginAPI_setGlobalPanicCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
);

#ifdef __cplusplus
}
#endif
