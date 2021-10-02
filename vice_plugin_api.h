#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VICE_PLUGIN_API_FUNC_DECLSPEC
#define VICE_PLUGIN_API_FUNC_DECLSPEC
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
 * and file downloads and uploads through the plugin is supported by the API.
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
 *         like this, the program should call vicePluginAPI_pumpEvents as soon as possible. The
 *         implementation of vicePluginAPI_pumpEvents may then advance the task queue of the plugin
 *         and call the callbacks provided by the program directly.
 *
 *     While the plugin context is running, the following kinds of things happen:
 *
 *       - The plugin opens and closes windows using the createWindow and closeWindow callbacks.
 *
 *       - The program opens popup windows from existing windows (if allowed by the plugin) and
 *         closes windows by calling vicePluginAPI_createPopupWindow and vicePluginAPI_closeWindow.
 *
 *       - The program supplies the window view image for each open window whenever the plugin
 *         requests it using the fetchWindowImage callback. The program notifies the plugin whenever
 *         the window image has changed by calling vicePluginAPI_notifyWindowViewChanged.
 *
 *       - The plugin sends various events to the program by calling callbacks, such as
 *           - window view resize requests with the resizeWindow callback,
 *           - input events using mouse*, key* and loseFocus callbacks.
 *         The program processes these events in an application-specific manner.
 *
 *       - Communication for other features, such as file uploads and downloads, mouse cursor
 *         updates, clipboard text, optional plugin navigation buttons and program widgets; see the
 *         function documentation comments for details.
 *
 *  5. To initiate the shutdown of the plugin context, the program must call vicePluginAPI_shutdown.
 *     When the plugin has shut down, it will respond by calling the shutdownComplete callback (in
 *     vicePluginAPI_pumpEvents). After this, the program and the plugin must immediately cease all
 *     communication for this context.
 *
 *  6. The program destroys the plugin context using vicePluginAPI_destroyContext.
 *
 * API version 1000001 adds support for extensions; see the section "API version 1000001" for more
 * information.
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
VICE_PLUGIN_API_FUNC_DECLSPEC int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion);

/* Returns a string describing the name and version of the plugin. The caller is responsible for
 * freeing the string using free().
 */
VICE_PLUGIN_API_FUNC_DECLSPEC char* vicePluginAPI_getVersionString();

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
     * thus it must not be used in any subsequent API/callback calls (including the API function
     * vicePluginAPI_closeWindow).
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

    /* If the UI for a window presented to the user by the plugin contains navigation
     * (back/refresh/forward) controls (such as buttons), the plugin may relay their events to the
     * program using this function. The direction argument must be set to -1 for back and 1 for
     * forward navigation, and 0 for refresh. The program may ignore these events.
     */
    void (*navigate)(void*, uint64_t window, int direction);

    /* Called by the plugin to request that a given text should be copied to the clipboard of the
     * program. While the text should be encoded as UTF-8, arbitrary null-terminated binary data is
     * still allowed; the program should either tolerate invalid UTF-8 or validate/sanitize the data
     * before use. The program may ignore this request, allow it only in specific circumstances or
     * process it in any application-specific manner.
     */
    void (*copyToClipboard)(void*, const char* text);

    /* Called by the plugin to request that the program should send the contents of its clipboard to
     * the plugin by calling vicePluginAPI_putClipboardContent as soon as possible. If the program
     * accepts the request, the function must return 1; otherwise the function must return 0 (for
     * example if the program has no clipboard support). If the plugin waits for the clipboard
     * content, it should have a reasonable timeout (such as 1 second) after which it aborts the
     * wait, as the program may take a long time to fetch the clipboard. The program may respond
     * multiple requestClipboardContent calls with only a single call to
     * vicePluginAPI_putClipboardContent.
     */
    int (*requestClipboardContent)(void*);

    /* Uploads a file to the program. May only be called when the window is in file upload mode
     * started by vicePluginAPI_startFileUpload. This function ends the file upload mode (and thus
     * the program should not call vicePluginAPI_cancelFileUpload). After this, the modal file
     * upload dialog should be closed. The data of the file must be available in a readable local
     * file with given path. Once the program does not need the file anymore, it must call the
     * cleanup function with given cleanupData as the only argument from any thread at any time. The
     * program must call the cleanup function exactly once, and it must do so before the context is
     * destroyed. The program may only read the file; it must not modify, move or remove it. The
     * name argument specifies the suggested name for the file, which may be an arbitrary
     * null-terminated string; the program may sanitize the name or even ignore it.
     */
    void (*uploadFile)(
        void*,
        uint64_t window,
        const char* name,
        const char* path,
        void (*cleanup)(void*),
        void* cleanupData
    );

    /* Ends a currently active file upload mode (started by vicePluginAPI_startFileUpload) for given
     * window by canceling the upload. The plugin should close the modal file upload dialog. As this
     * call ends the file upload mode, the program should not call vicePluginAPI_cancelFileUpload.
     */
    void (*cancelFileUpload)(void*, uint64_t window);

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

    /* Invalid value that is larger than any valid enum value, used to ensure
     * binary compatibility when new values are added.
     */
    VICE_PLUGIN_API_LOG_LEVEL_HUGE_UNUSED = 1000000000
};
typedef enum VicePluginAPI_LogLevel VicePluginAPI_LogLevel;

/* Type of mouse cursors used in vicePluginAPI_setWindowCursor. */
enum VicePluginAPI_MouseCursor {
    VICE_PLUGIN_API_MOUSE_CURSOR_NORMAL = 0,
    VICE_PLUGIN_API_MOUSE_CURSOR_HAND = 1,
    VICE_PLUGIN_API_MOUSE_CURSOR_TEXT = 2,

    /* Invalid value that is larger than any valid enum value, used to ensure
     * binary compatibility when new values are added.
     */
    VICE_PLUGIN_API_MOUSE_CURSOR_HUGE_UNUSED = 1000000000
};
typedef enum VicePluginAPI_MouseCursor VicePluginAPI_MouseCursor;

/**************************************
 * General context handling functions *
 **************************************/

/* Initializes a new plugin context with configuration options given as name-value-pairs
 * (optionNames[i], optionValues[i]) for 0 <= i < optionCount, returning the created context on
 * success. (Documentation of the supported configuration options can be queried using
 * vicePluginAPI_getOptionDocs.)
 *
 * The programName argument should specify the name of the program that the plugin may display to
 * the user if appropriate. The plugin should not make any assumptions about the name and sanitize
 * it as required. To achieve the best compatibility, the program name should be a short string
 * consisting only of ASCII letters, numbers and spaces, as some plugins may have to filter out
 * special characters and the space may be limited.
 *
 * In case of failure, NULL is returned and if initErrorMsg is not NULL, *initErrorMsg is set to
 * point to a string describing the reason for the failure; the caller must free the string using
 * free().
 *
 * The program may attempt create multiple independent contexts for the same plugin; if the plugin
 * does not support this and the program attempts to create a second context, this function should
 * fail with a descriptive error message.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC VicePluginAPI_Context* vicePluginAPI_initContext(
    uint64_t apiVersion,
    const char** optionNames,
    const char** optionValues,
    size_t optionCount,
    const char* programName,
    char** initErrorMsgOut
);

/* Destroy a vice plugin context that was previously initialized successfully by
 * vicePluginAPI_initContext. If the context has been started using vicePluginAPI_start, it must
 * be successfully shut down prior to calling this function (by calling vicePluginAPI_shutdown and
 * waiting for the shutdownComplete callback to be called).
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_destroyContext(VicePluginAPI_Context* ctx);

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
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_start(
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
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_shutdown(VicePluginAPI_Context* ctx);

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
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_pumpEvents(VicePluginAPI_Context* ctx);

/**********************************************
 * API functions to use with running contexts *
 **********************************************/

/* The following functions may be called by the program for contexts that are running, i.e. contexts
 * that have been started with vicePluginAPI_start and have not yet shut down (the plugin has not
 * called the shutdownComplete callback yet).
 */

/* Create a popup window with handle popupWindow (a nonzero uint64_t value that is not already in
 * use by a window) from an existing window parentWindow. To allow the creation of the window, the
 * function must return 1 and ignore msg; the created window begins its existence immediately. The
 * created window works in exactly the same way as windows created by the plugin, and it exists
 * independently of parentWindow. To deny the creation of the window, the function must return 0 and
 * if msg is not NULL, it must point *msg to a short human-readable string describing the reason for
 * the denial; the calling program is responsible for freeing the string using free().
 */
VICE_PLUGIN_API_FUNC_DECLSPEC int vicePluginAPI_createPopupWindow(
    VicePluginAPI_Context* ctx,
    uint64_t parentWindow,
    uint64_t popupWindow,
    char** msg
);

/* Close an existing window. The window stops existing immediately and thus it must not be used in
 * any subsequent API/callback calls (including the closeWindow callback).
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_closeWindow(
    VicePluginAPI_Context* ctx,
    uint64_t window
);

/* Notifies the plugin that the view in an existing window has changed. After receiving this
 * notification, the plugin should use the fetchWindowImage callback to fetch the new view image and
 * show it to the user as soon as possible.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_notifyWindowViewChanged(
    VicePluginAPI_Context* ctx,
    uint64_t window
);

/* Changes the currently active mouse cursor for given window. */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_setWindowCursor(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    VicePluginAPI_MouseCursor cursor
);

/* If the plugin needs a quality selector for given existing window, returns 1 and sets
 * *qualityListOut and *currentQualityOut to describe the possible options and the currently
 * selected option for the selector. Otherwise, returns 0 and ignores qualityListOut and
 * currentQualityOut. The program is recommended to call this function for every window and if the
 * result is 1, display a quality selector widget in the UI for the window with the specified
 * options, relaying selection events to the plugin by calling vicePluginAPI_windowQualityChanged.
 * However, the program is also allowed to not call this function at all and omit the quality
 * selector.
 *
 * If the function returns 1, it must point *qualityListOut to a null-terminated string that
 * contains a concatenated list of quality option labels delimited by newline characters. Each
 * quality label must be a string of 1-3 ASCII characters in range 33..126. There must be at least
 * one quality label. Duplicate labels are not recommended but are allowed. Each quality label must
 * be followed by a single newline character ('\n'), including the last quality label. The calling
 * program is responsible for freeing the string *qualityListOut using free(). The function must
 * point *currentQualityOut to a valid 0-based index for the list of quality options. By convention,
 * the quality options should be ordered from the worst (fastest) to the best (slowest).
 *
 * For example, if there are four qualities, "Bad", "OK", "HD" and "5/5", and "OK" is the default,
 * the function should return 1, set *qualityListOut to point to a new string "Bad\nOK\nHD\n5/5\n"
 * and set *currentQualityOut to 1.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC int vicePluginAPI_windowQualitySelectorQuery(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    char** qualityListOut,
    size_t* currentQualityOut
);

/* Called by the program to notify the plugin that the user has selected the quality with index
 * qualityIdx in the quality selector widget of given window. The index qualityIdx must be a valid
 * 0-based index to the list of quality options that were provided by the previous call to
 * vicePluginAPI_windowQualitySelectorQuery for this window.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_windowQualityChanged(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    size_t qualityIdx
);

/* Returns 1 if the plugin needs a clipboard button for given existing window; otherwise, returns 0.
 * The program is recommended to call this function for every window and if the result is 1, display
 * a clipboard button in the UI for the window (click events are relayed by calling
 * vicePluginAPI_windowClipboardButtonPressed). However, the program is also allowed to not call
 * this function at all or to display/omit a clipboard button independent of the result.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC int vicePluginAPI_windowNeedsClipboardButtonQuery(
    VicePluginAPI_Context* ctx,
    uint64_t window
);

/* Signals to the plugin that the clipboard button in a window has been pressed. May be called even
 * if vicePluginAPI_windowNeedsClipboardButtonQuery returns 0.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_windowClipboardButtonPressed(
    VicePluginAPI_Context* ctx,
    uint64_t window
);

/* Sends the content of the program clipboard to the plugin. Typically called after the plugin has
 * requested the content using the requestClipboardContent callback; however, the program is allowed
 * to call this function even if not requested to do so. While the encoding of the text should be
 * UTF-8, arbitrary null-terminated binary data is still allowed, and thus the plugin should either
 * tolerate invalid UTF-8 or validate/sanitize the data before use.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_putClipboardContent(
    VicePluginAPI_Context* ctx,
    const char* text
);

/* Sends the plugin a file which the plugin may then allow the user to download through given
 * window. The data of the file must be available in a readable local file with given path. Once the
 * plugin does not need the file anymore, it must call the cleanup function with given cleanupData
 * as the only argument from any thread at any time. The plugin must call the cleanup function
 * exactly once, and it must do so before the context is destroyed. The plugin may only read the
 * file; it must not modify, move or remove it. The name argument specifies the suggested name for
 * the file, which may be an arbitrary null-terminated string; the plugin may sanitize the name or
 * even ignore it. One valid implementation that ignores all downloads is { cleanup(cleanupData); }.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_putFileDownload(
    VicePluginAPI_Context* ctx,
    uint64_t window,
    const char* name,
    const char* path,
    void (*cleanup)(void*),
    void* cleanupData
);

/* If the function returns 1, it starts the file upload mode for an existing window, where the
 * plugin should display a dialog (or similar) to select a file to upload to the program. This
 * dialog should be modal, which means that the user should be prevented from using the window
 * normally until the file upload mode is over, and the attention of the user should be pointed to
 * the upload dialog. However, the plugin does not need to enforce this: it may still continue to
 * relay input events to the program from the window. This function may not be called if the window
 * is already in file upload mode. The file upload mode will end if the program cancels it by
 * calling vicePluginAPI_cancelFileUpload or if the plugin uploads a file with the uploadFile
 * callback or cancels the upload using the cancelFileUpload callback. The window may also be closed
 * normally while in file upload mode; in that case, the file upload mode does not have to be ended
 * separately. To deny the file upload, the plugin may return 0 from this function (for example if
 * it does not support file uploads); in that case, the file upload mode is not started.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC int vicePluginAPI_startFileUpload(
    VicePluginAPI_Context* ctx,
    uint64_t window
);

/* Ends a currently active file upload mode (started by vicePluginAPI_startFileUpload) for given
 * window by canceling the upload. The plugin should close the modal file upload dialog without
 * calling any of the upload callbacks.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_cancelFileUpload(
    VicePluginAPI_Context* ctx,
    uint64_t window
);

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
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_getOptionDocs(
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
 *   - logLevel: The severity of the log event.
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
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_setGlobalLogCallback(
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
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_setGlobalPanicCallback(
    uint64_t apiVersion,
    void (*callback)(void* data, const char* location, const char* msg),
    void* data,
    void (*destructorCallback)(void* data)
);

/***************************************************************************************************
 *** API version 1000001 ***
 ***************************/

/* API version 1000001 contains all the same functionality as API version 1000000, and adds support
 * for API extensions through the function vicePluginAPI_isExtensionSupported.
 */

/* Returns 1 if the vice plugin supports API extension with given name (null-terminated and case
 * sensitive), and 0 otherwise. This function may be called at any time from any thread, and the
 * same plugin should always return the same result for the same extension name. If the return value
 * is 1, the program may use the functions for that extensions as documented below or in other
 * sources. Avoidance of name conflicts should be kept in mind when naming new extensions;
 * organization names or other identifiers may be added as necessary, and extension function names
 * should start with vicePluginAPI_EXTNAME_ (where EXTNAME is replaced by the name of the extension)
 * where possible.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC int vicePluginAPI_isExtensionSupported(
    uint64_t apiVersion,
    const char* name
);

/***************************************************************************************************
 *** API extension "URINavigation" ***
 *************************************/

/* Extension that allows the plugin to navigate windows (both existing and newly created) to
 * arbitrary URIs (Uniform Resource Identifiers) through two additional callbacks in the
 * VicePluginAPI_URINavigation_Callbacks structure. The extension is enabled by the program using
 * vicePluginAPI_URINavigation_enable. The program should be able to handle arbitrary
 * null-terminated binary data in the URI strings given by the plugin in addition to valid URIs,
 * validating and sanitizing the strings if necessary.
 */

struct VicePluginAPI_URINavigation_Callbacks {
    /* Variant of createWindow in VicePluginAPI_Callbacks that requests that the created window is
     * initially navigated to given URI.
     */
    uint64_t (*createWindowWithURI)(void*, char** msg, const char* uri);

    /* Called by the plugin to request that given existing window navigates to given URI. */
    void (*navigateWindowToURI)(void*, uint64_t window, const char* uri);
};
typedef struct VicePluginAPI_URINavigation_Callbacks VicePluginAPI_URINavigation_Callbacks;

/* Enables the URINavigation callbacks in given context. May only be called once for each context,
 * after vicePluginAPI_initContext and before vicePluginAPI_start. The vice plugin uses the
 * callbacks similarly to the callbacks given in vicePluginAPI_start.
 */
VICE_PLUGIN_API_FUNC_DECLSPEC void vicePluginAPI_URINavigation_enable(
    VicePluginAPI_Context* ctx,
    VicePluginAPI_URINavigation_Callbacks callbacks
);

#ifdef __cplusplus
}
#endif
