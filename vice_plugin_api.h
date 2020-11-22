#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************************
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
 * and mouse events concerning the windows back to the program. New windows can be initiated both by
 * the plugin and the program. The program may send files to the user through the plugin. The
 * program may also allow the user to access its clipboard through the plugin.
 *
 * Typical API usage example for API version 1000000:
 *
 *  1. The program verifies that the plugin supports API version 1000000 by calling
 *     vicePluginAPI_isAPIVersionSupported.
 *
 *  2. (optional) The program registers its logging and panicking functions to the plugin using
 *     vicePluginAPI_setLogCallback and vicePluginAPI_setPanicCallback, allowing the plugin to use
 *     them instead of its default logging and panicking behavior.
 *
 *  3. TODO
 *
 * General API guidelines:
 *
 *   - No function in the API may use pointers given to it as arguments after the function has
 *     returned, unless specifically allowed by the documentation. This applies to functions in the
 *     plugin API as well as callback functions given to the plugin through the API.
 */

/********************************************
 *** Functions common to all API versions ***
 ********************************************/

/* Returns 1 if the plugin supports the given API version; otherwise, returns 0. */
int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion);

/*****************
 *** Constants ***
 *****************/

#define VICE_PLUGIN_API_LOG_LEVEL_INFO 0
#define VICE_PLUGIN_API_LOG_LEVEL_WARNING 10
#define VICE_PLUGIN_API_LOG_LEVEL_ERROR 20

/*****************************************
 *** Functions for API version 1000000 ***
 *****************************************/

/* The following two functions may be called to allow the plugin to use given callback for logging
 * or panicking instead of the default behavior. After this, the plugin may call the given callback
 * from any thread at any time.
 *
 * The logging callback should log the given message with the appropriate context information for
 * the user to see. The panicking callback must never return and it must end the process in a timely
 * manner (for example using abort()).
 *
 * Arguments for callback:
 *   - data: The data argument given to vicePluginAPI_set*Callback.
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
void vicePluginAPI_setLogCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, int logLevel, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
);
void vicePluginAPI_setPanicCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
);

#ifdef __cplusplus
}
#endif
