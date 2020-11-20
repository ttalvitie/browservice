#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************************
 *** Vice Plugin API Definition ***
 **********************************/

/* A vice plugin is a shared library that is used by Browservice or any other GUI software to show
 * its GUI to the user and receive input events from the user. This plugin API is designed to make
 * it easy to implement new vice plugins that enable the same program to show its the GUI in very
 * different ways, such as:
 *   - A HTTP server serving the GUI as a web application
 *   - Relay server for clients running on obsolete operating systems and hardware
 *   - A GUI program running in the native graphical environment
 *
 * The GUI consists of multiple windows; the program supplies the plugin with updates to a resizable
 * 24-bit RGB image view for each window, and the plugin sends the keyboard and mouse events from
 * the user in each window back to the program. New windows can be initiated and closed by both the
 * program and the plugin.
 *
 * General API guidelines:
 *
 *   - The program using the plugin must use vicePluginAPI_isAPIVersionSupported to find an API
 *     version that both the program and the plugin support. In other plugin API functions, setting
 *     the apiVersion argument to an unsupported version or using API functions that are not part of
 *     the chosen API version results in undefined behavior.
 *
 *   - Every callback argument to an API function is followed by a data argument of type void*. The
 *     plugin must pass this data argument as the first argument when calling the callback.
 *
 *   - The implementation of a plugin API function or a callback function passed to a plugin API
 *     function must not retain pointers to their arguments after the function has returned,
 *     unless specifically allowed by the documentation.
 *
 *   - To make implementing new plugins easier, the burden of avoiding concurrency issues is shifted
 *     to the program using the API by the means of the following rules:
 *
 *       - The program must synchronize its calls to the API functions such that two calls
 *         concerning the same VicePluginAPI_Context are never running concurrently.
 *
 *       - The program may never call API functions from a thread currently executing a callback
 *         function called by the plugin.
 *
 *       - The plugin may call callbacks given to it by the program in ANY thread, unless stated
 *         otherwise in the API documentation. This means that, for example, the plugin may call
 *         multiple callback functions concurrently, or call any callback from a thread currently
 *         executing any plugin API function.
 *
 *     To ensure that the rules are followed and issues such as data races, deadlocks and infinite
 *     recursion are avoided, the program using the plugin is recommended to implement its callbacks
 *     such that the resulting actions are deferred using a task queueing mechanism where possible.
 *
 *   - This API is pure C; API functions and callbacks must not let C++ exceptions fall through to
 *     the caller.
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

/* For each configuration option supported for vicePluginAPI_initContext, calls itemCallback
 * immediately in the current thread with the documentation for that option:
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
 * created context on success. In case of failure, NULL is returned and if initErrorMsg is not NULL,
 * *initErrorMsg is set to point to a string describing the reason for the failure; the caller must
 * free the string using free(). If creating the context is successful, the other callbacks,
 * panicCallback and logCallback, are retained in the returned context and may be called by the
 * context at any point before the context is destroyed. Both panicCallback and logCallback should
 * attempt to show the given message to the user. A call to panicCallback must not return; instead,
 * it must end the process immediately (for example by calling abort()). Arguments for the
 * callbacks:
 *   - msg: Textual description of the event/error.
 *   - location: The source of the error, such as "vice.cpp:132".
 *   - severity: The importance of the log message: 0 for INFO, 1 for WARNING and 2 for ERROR.
 */
VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    char** initErrorMsgOut,
    void (*panicCallback)(void*, const char* location, const char* msg),
    void* panicCallbackData,
    void (*logCallback)(void*, int severity, const char* location, const char* msg),
    void* logCallbackData
);

/* Start running a previously initialized plugin context. After this, the plugin will start
 * actually running its functionality: creating and managing interactive windows, communicating
 * with the program using the plugin through the provided callbacks and plugin API functions. This
 * function must return immediately (which means that the plugin must work in a background thread).
 * To shut down the context, call vicePluginAPI_asyncShutdownContext and wait for it to complete
 * before destroying the context using vicePluginAPI_destroyContext. A context may be started only
 * once during its lifetime.
 */
void vicePluginAPI_startContext(VicePluginAPI_Context* ctx);

/* Shut down a context that is running due to being previously started using
 * vicePluginAPI_startContext. This function will return immediately, and the plugin context will be
 * shut down in the background. The callback shutdownCompleteCallback is called when the plugin
 * context has been successfully shut down and is ready to be destroyed. After calling this
 * function and before shutdownCompleteCallback is called, plugin API functions may not be called
 * for this context. However, the plugin may still call callbacks as if the plugin was still running
 * normally prior to calling shutdownCompleteCallback.
 */
void vicePluginAPI_asyncShutdownContext(
    VicePluginAPI_Context* ctx,
    void (*shutdownCompleteCallback)(void*),
    void* shutdownCompleteCallbackData
);

/* Destroy given vice plugin context previously initialized by vicePluginAPI_initContext. If the
 * plugin has been started with vicePluginAPI_startContext, it must first be shut down by calling
 * vicePluginAPI_asyncShutdownContext and waiting for it to signal its completion.
 */
void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx);

#ifdef __cplusplus
}
#endif
