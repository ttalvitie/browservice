#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
 *** Vice Plugin API Definition ***
 **********************************/

/* This file describes the common API for "vice plugins": shared libraries that can be used by an
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
 *  4. The program starts the operation of the plugin context by calling vicePluginAPI_start,
 *     providing a VicePluginAPI_Callbacks structure that contains function pointers to callbacks to
 *     be called by the plugin. After this, the program and plugin communicate using function calls
 *     as follows:
 *
 *       - The program may call API functions directly; however, it must never make two API function
 *         calls concerning the same context concurrently.
 *
 *       - To avoid concurrency and reentrancy issues, the plugin context must not call callbacks
 *         directly from its background threads or API functions invoked by the program (except when
 *         specifically permitted by the documentation). Instead, it is synchronized to the program
 *         event loop as follows: The plugin notifies the program when it has events to process by
 *         calling the special eventNotify callback in any thread at any time. After a notification
 *         like this, the program should call vicePlugin_pumpEvents as soon as possible. The
 *         implementation of vicePlugin_pumpEvents may then advance the task queue of the plugin and
 *         call the callbacks provided by the program directly.
 *
 *     While the plugin context is running, the following kinds of things happen:
 *
 *       - The plugin opens and closes windows using the createWindow and closeWindow callbacks.
 *
 *       - The program supplies the window view image for each open window whenever the plugin
 *         requests it using the fetchWindowImage callback. The program notifies the plugin whenever
 *         the window image has changed by calling vicePluginAPI_notifyWindowViewChanged.
 *
 *       - The plugin sends various events to the program by calling callbacks, such as
 *           - window view resize requests with the resizeWindow callback,
 *           - user keyboard and mouse input events using TODO.
 *         The program processes these events in an application-specific manner.
 *
 *  5. To initiate the shutdown of the plugin context, the program must call vicePluginAPI_shutdown.
 *     When the plugin has shut down, it will respond by calling the shutdownComplete callback (in
 *     vicePlugin_pumpEvents). After this, the program and the plugin must immediately cease all
 *     communication for this context.
 *
 *  6. The program destroys the plugin context using vicePluginAPI_destroyContext.
 *
 * General API conventions and rules:
 *
 *   - The program and plugin communicate bidirectionally using function calls. The program directly
 *     calls the API functions of the plugin, and the plugin calls callback function pointers
 *     supplied by the program. In all its calls to the callback functions, the plugin passes a void
 *     data pointer supplied by the program as the first argument. The program may use this pointer
 *     to access its own data structures instead of using global variables.
 *
 *   - To avoid reentrancy issues,
 *
 *       * implementations of the plugin API functions may not directly call callbacks supplied by
 *         the program, and
 *
 *       * implementations of the callbacks may not directly call plugin API functions
 *
 *     except when specifically allowed by the documentation. To get around this restriction, a task
 *     queue should be used to defer the calls.
 *
 *   - No function in the API may retain and use pointers given to it as arguments after the
 *     function has returned, unless specifically allowed by the documentation. This applies to
 *     functions in the plugin API as well as callback functions given to the plugin through the
 *     API.
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

/***************************************************************************************************
 *** API version 1000000 ***
 ***************************/

/*********
 * Types *
 *********/

/* Opaque type for plugin contexts. */
typedef struct VicePluginAPI_Context VicePluginAPI_Context;

/* Struct of pointers to callback functions provided by the program to be called by a running
 * plugin. The program provides this struct to the plugin as an argument of vicePluginAPI_start
 * along with a callbackData void* pointer that the plugin always passes as the first argument to
 * each callback. With the exception of eventNotify, these callbacks are only called by the plugin
 * from a thread executing a vicePluginAPI_pumpEvents call invoked by the program.
 */
struct VicePluginAPI_Callbacks {

    /* A notification function that may be called by a running plugin from any thread at any time
     * (even from an API function invoked by the program) to notify that the plugin has new events
     * to process. When receiving this notification, the program must ensure that
     * vicePluginAPI_pumpEvents is called as soon as possible; this will allow the plugin to process
     * its internal event queue and call the other callbacks. Note that the program may not call
     * vicePluginAPI_pumpEvents or any other API function from the callback itself.
     */
    void (*eventNotify)(void*);

    /* Called by the plugin when the plugin shutdown (initiated by the program using
     * vicePluginAPI_shutdown) is complete; after this, the context is no longer running. This means
     * that it will not call any further callbacks (including eventNotify) and the program is not
     * allowed call any other API functions for the context except for vicePluginAPI_destroyContext.
     */
    void (*shutdownComplete)(void*);

    /* Called by the plugin to request the creation of a new window. To allow the creation of the
     * window, the function must return a handle for the new window (a nonzero uint64_t value that
     * is not already in use by a window) and ignore msg; the window begins its existence
     * immediately, and the returned handle is used to identify it in subsequent API and callback
     * calls. To deny the creation of the window, the function must return 0 and if msg is not NULL,
     * it must point *msg to a short human-readable string describing the reason for the denial; the
     * plugin is responsible for freeing the string using free().
     */
    uint64_t (*createWindow)(void*, char** msg);

    /* Called by the plugin to close an existing window. The window stops existing immediately and
     * thus it must not be used in any subsequent API calls.
     */
    void (*closeWindow)(void*, uint64_t window);

    /* Called by the plugin to request that the view image size for given window should be
     * width x height, where width > 0 and height > 0. While the program is not required to obey the
     * request in subsequent fetchWindowImage calls, it should attempt to follow the request as
     * closely as possible as soon as possible. Typically, the plugin should call this function
     * after the creation of each window (in addition to window resizes) because this is the only
     * way for the plugin to signal its preference on the window view size.
     */
    void (*resizeWindow)(void*, uint64_t window, size_t width, size_t height);

    /* Called by the plugin to fetch the newest available view image of a window for rendering. The
     * function must call the supplied callback putImageFunc exactly once before returning. The
     * callback must pass the given data pointer as the first argument to putImageFunc, and use the
     * rest of the arguments (image, width, height, pitch) to specify the image data:
     *
     *   - It must always hold that width > 0 and height > 0.
     *
     *   - For all 0 <= y < height and 0 <= x < width, image[4 * (y * pitch + x) + c] is the value at
     *     image position (x, y) for color channel blue, green and red for c = 0, 1, 2, respectively.
     *     The values for channel c = 3 is not used for the image, but they must be safe to read.
     *
     * The program may not call putImageFunc after this function has returned. The plugin may not
     * use the image pointer after putImageFunc has returned; it should either render the image
     * immediately or copy it to an internal buffer.
     *
     * The plugin does not need to poll this function to detect changes to the window view; the
     * program must use vicePluginAPI_notifyWindowViewChanged to notify the plugin whenever an
     * updated view is available through this function.
     */
    void (*fetchWindowImage)(
        void*,
        uint64_t window,
        void (*putImageFunc)(
            void* data,
            const uint8_t* image,
            size_t width,
            size_t height,
            size_t pitch
        ),
        void* data
    );

    /* Called by the plugin to relay window input events (mouse, keyboard and focus) from the user.
     * Processing input events is a messy business, and thus the implementations of these callbacks
     * must tolerate all possible values for the int arguments and inconsistent state changes (e.g.
     * huge values and negative values, mouse moving outside the window, invalid key codes, key
     * repeat without key up event in between), clamping the values or ignoring the events where
     * appropriate.
     *
     * Guidelines for event interpretation:
     *
     *   - Left, middle and right mouse button numbers are 0, 1 and 2, respectively.
     *
     *   - Mouse wheel delta is positive for scrolling down/right, negative for scrolling up/left.
     *     The delta for one line of text is in the ballpark of 20.
     *
     *   - Positive key codes correspond to character keys. Each positive key code is equal to the
     *     Unicode code point of the corresponding (modified) character.
     *
     *   - Negative key codes correspond to non-character keys. Each negative key code is the
     *     negation of the corresponding Windows key code. The enum VicePluginAPI_Key contains a
     *     non-exhaustive list of the most important non-character key codes.
     */
    void (*mouseDown)(void*, uint64_t window, int x, int y, int button);
    void (*mouseUp)(void*, uint64_t window, int x, int y, int button);
    void (*mouseMove)(void*, uint64_t window, int x, int y);
    void (*mouseDoubleClick)(void*, uint64_t window, int x, int y, int button);
    void (*mouseWheel)(void*, uint64_t window, int x, int y, int dx, int dy);
    void (*mouseLeave)(void*, uint64_t window, int x, int y);
    void (*keyDown)(void*, uint64_t window, int key);
    void (*keyUp)(void*, uint64_t window, int key);
    void (*loseFocus)(void*, uint64_t window);

};
typedef struct VicePluginAPI_Callbacks VicePluginAPI_Callbacks;

/* A non-exhaustive list of the most important non-character key codes (negations of the
 * corresponding Windows key codes), as used in the keyDown and keyUp callbacks.
 */
enum VicePluginAPI_Key {
    VICE_PLUGIN_API_KEY_BACKSPACE = -8,
    VICE_PLUGIN_API_KEY_TAB = -9,
    VICE_PLUGIN_API_KEY_ENTER = -13,
    VICE_PLUGIN_API_KEY_SHIFT = -16,
    VICE_PLUGIN_API_KEY_CONTROL = -17,
    VICE_PLUGIN_API_KEY_ALT = -18,
    VICE_PLUGIN_API_KEY_CAPSLOCK = -20,
    VICE_PLUGIN_API_KEY_ESC = -27,
    VICE_PLUGIN_API_KEY_SPACE = -32,
    VICE_PLUGIN_API_KEY_PAGEUP = -33,
    VICE_PLUGIN_API_KEY_PAGEDOWN = -34,
    VICE_PLUGIN_API_KEY_END = -35,
    VICE_PLUGIN_API_KEY_HOME = -36,
    VICE_PLUGIN_API_KEY_LEFT = -37,
    VICE_PLUGIN_API_KEY_UP = -38,
    VICE_PLUGIN_API_KEY_RIGHT = -39,
    VICE_PLUGIN_API_KEY_DOWN = -40,
    VICE_PLUGIN_API_KEY_INSERT = -45,
    VICE_PLUGIN_API_KEY_DELETE = -46,
    VICE_PLUGIN_API_KEY_WIN = -91,
    VICE_PLUGIN_API_KEY_MENU = -93,
    VICE_PLUGIN_API_KEY_F1 = -112,
    VICE_PLUGIN_API_KEY_F2 = -113,
    VICE_PLUGIN_API_KEY_F3 = -114,
    VICE_PLUGIN_API_KEY_F4 = -115,
    VICE_PLUGIN_API_KEY_F5 = -116,
    VICE_PLUGIN_API_KEY_F6 = -117,
    VICE_PLUGIN_API_KEY_F7 = -118,
    VICE_PLUGIN_API_KEY_F8 = -119,
    VICE_PLUGIN_API_KEY_F9 = -120,
    VICE_PLUGIN_API_KEY_F10 = -121,
    VICE_PLUGIN_API_KEY_F11 = -122,
    VICE_PLUGIN_API_KEY_F12 = -123,
    VICE_PLUGIN_API_KEY_NUMLOCK = -144
};
typedef enum VicePluginAPI_Key VicePluginAPI_Key;

/* Type of log levels used in vicePluginAPI_setGlobalLogCallback. */
enum VicePluginAPI_LogLevel {
    VICE_PLUGIN_API_LOG_LEVEL_INFO = 100,
    VICE_PLUGIN_API_LOG_LEVEL_WARNING = 200,
    VICE_PLUGIN_API_LOG_LEVEL_ERROR = 300,

    /* Value larger than any other enum value. Included in the enum to ensure binary compatibility
     * when new values are added. Should NOT be used as a log level.
     */
    VICE_PLUGIN_API_LOG_LEVEL_HUGE_UNUSED = 1000000000
};
typedef enum VicePluginAPI_LogLevel VicePluginAPI_LogLevel;

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

/* Destroy a vice plugin context that was previously initialized successfully by
 * vicePluginAPI_initContext. If the context has been started using vicePluginAPI_start, it must
 * be successfully shut down prior to calling this function (by calling vicePluginAPI_shutdown and
 * waiting for the shutdownComplete callback to be called).
 */
void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx);

/* Start running given plugin context. This function may be called only once per context. All the
 * fields of the given callbacks structure must be populated with valid function pointers (NULL
 * function pointers are not allowed).
 *
 * During and after this function call, the running plugin may call callbacks.eventNotify from any
 * thread at any time. When receiving such call, the program must ensure that
 * vicePluginAPI_pumpEvents is called as soon as possible. In vicePluginAPI_pumpEvents, the plugin
 * may then call the other callbacks supplied in the callbacks structure. The plugin always passes
 * callbackData as the first argument to each function in the callbacks structure.
 *
 * To shut down the plugin, the program must call vicePluginAPI_shutdown and continue running
 * normally until callbacks.shutdownComplete is called. After this, both the program and the plugin
 * must immediately stop calling API functions and callbacks for this context (except for
 * vicePluginAPI_destroyContext).
 */
void vicePluginAPI_start(
    VicePluginAPI_Context* ctx,
    VicePluginAPI_Callbacks callbacks,
    void* callbackData
);

/* Initiate the shutdown of a context that was previously started using vicePluginAPI_start. This
 * function may be called only once per context. When the shutdown is complete, the plugin will call
 * the shutdownComplete callback (in vicePluginAPI_pumpEvents); after this, the plugin will not call
 * any further callbacks and the program must destroy the context using vicePluginAPI_destroyContext
 * (and not call any other API functions for this context).
 */
void vicePluginAPI_shutdown(VicePluginAPI_Context* ctx);

/* Allows a running plugin context to make progress in its own task queue. May call callbacks
 * (supplied by the program in the callbacks argument of vicePluginAPI_start) directly in the
 * current thread before returning.
 *
 * This function should be called by the program if the eventNotify callback has been called after
 * this function was invoked the last time. Note that eventNotify may also be called while this
 * function is executing; in that case, this function should be called again.
 *
 * The program is allowed to call this function even when eventNotify has not been called. It is
 * sufficient for the program to call this function only once even if eventNotify has been called
 * multiple times after the previous call to this function.
 */
void vicePluginAPI_pumpEvents(VicePluginAPI_Context* ctx);

/**********************************************
 * API functions to use with running contexts *
 **********************************************/

/* The following functions may be called for contexts that are running, i.e. contexts that have been
 * started with vicePluginAPI_start and have not yet shut down (the plugin has not called the
 * shutdownComplete callback yet).
 */

/* Called to notify the plugin that the view in an existing window has changed. After receiving this
 * notification, the plugin should use the fetchWindowImage callback to fetch the new view image and
 * show it to the user as soon as possible.
 */
void vicePluginAPI_notifyWindowViewChanged(VicePluginAPI_Context* ctx, uint64_t window);

/**********************************
 * Non-context-specific functions *
 **********************************/

/* Supplies the documentation for the configuration options supported by vicePluginAPI_initContext
 * by repeatedly calling given callback in the current thread before returning. Each call gives the
 * documentation for a single configuration option in its arguments:
 *   - data: The data argument given to vicePluginAPI_getOptionDocs.
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
 *   - data: The data argument given to vicePluginAPI_setGlobal*Callback.
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
 * callback is reset with another call to vicePluginAPI_setGlobal*Callback).
 *
 * The plugin may also choose to ignore the given callback completely. One valid implementation
 * for both of the functions is:
 * { if(callback && destructorCallback) destructorCallback(data); }
 */
void vicePluginAPI_setGlobalLogCallback(
    uint64_t apiVersion,
    void (*callback)(
        void* data,
        VicePluginAPI_LogLevel logLevel,
        const char* location,
        const char* msg
    ),
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
