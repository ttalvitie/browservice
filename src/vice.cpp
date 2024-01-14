#include "vice.hpp"

#include "download_manager.hpp"
#include "globals.hpp"
#include "temp_dir.hpp"
#include "widget.hpp"

#include "../vice_plugin_api.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace browservice {

struct VicePlugin::APIFuncs {
#define FOREACH_REQUIRED_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isAPIVersionSupported) \
    FOREACH_VICE_API_FUNC_ITEM(createVersionString) \
    FOREACH_VICE_API_FUNC_ITEM(createCreditsString) \
    FOREACH_VICE_API_FUNC_ITEM(malloc) \
    FOREACH_VICE_API_FUNC_ITEM(free) \
    FOREACH_VICE_API_FUNC_ITEM(initContext) \
    FOREACH_VICE_API_FUNC_ITEM(destroyContext) \
    FOREACH_VICE_API_FUNC_ITEM(start) \
    FOREACH_VICE_API_FUNC_ITEM(shutdown) \
    FOREACH_VICE_API_FUNC_ITEM(pumpEvents) \
    FOREACH_VICE_API_FUNC_ITEM(createPopupWindow) \
    FOREACH_VICE_API_FUNC_ITEM(closeWindow) \
    FOREACH_VICE_API_FUNC_ITEM(notifyWindowViewChanged) \
    FOREACH_VICE_API_FUNC_ITEM(setWindowCursor) \
    FOREACH_VICE_API_FUNC_ITEM(windowQualitySelectorQuery) \
    FOREACH_VICE_API_FUNC_ITEM(windowQualityChanged) \
    FOREACH_VICE_API_FUNC_ITEM(windowNeedsClipboardButtonQuery) \
    FOREACH_VICE_API_FUNC_ITEM(windowClipboardButtonPressed) \
    FOREACH_VICE_API_FUNC_ITEM(putClipboardContent) \
    FOREACH_VICE_API_FUNC_ITEM(putFileDownload) \
    FOREACH_VICE_API_FUNC_ITEM(startFileUpload) \
    FOREACH_VICE_API_FUNC_ITEM(cancelFileUpload) \
    FOREACH_VICE_API_FUNC_ITEM(getOptionDocs) \
    FOREACH_VICE_API_FUNC_ITEM(setGlobalLogCallback) \
    FOREACH_VICE_API_FUNC_ITEM(setGlobalPanicCallback) \
    FOREACH_VICE_API_FUNC_ITEM(isExtensionSupported)

#define FOREACH_VICE_API_FUNC \
    FOREACH_REQUIRED_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(URINavigation_enable) \
    FOREACH_VICE_API_FUNC_ITEM(PluginNavigationControlSupportQuery_query) \
    FOREACH_VICE_API_FUNC_ITEM(WindowTitle_enable) \
    FOREACH_VICE_API_FUNC_ITEM(WindowTitle_notifyWindowTitleChanged) \
    FOREACH_VICE_API_FUNC_ITEM(ZoomInput_enable) \
    FOREACH_VICE_API_FUNC_ITEM(VirtualKeyboardModeUpdate_update)

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    decltype(&vicePluginAPI_ ## name) name = nullptr;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM
};

namespace {

char* createMallocString(string val, void* (*mallocFunc)(size_t)) {
    size_t size = val.size() + 1;
    REQUIRE(mallocFunc != nullptr);
    char* ret = (char*)mallocFunc(size);
    REQUIRE(ret != nullptr);
    memcpy(ret, val.c_str(), size);
    return ret;
}

string pathToUTF8(PathStr path) {
#ifdef _WIN32
    try {
        wstring_convert<codecvt_utf8<wchar_t>> conv;
        return conv.to_bytes(path);
    } catch(const range_error&) {
        PANIC("Could not convert path to UTF-8");
        return "";
    }
#else
    return move(path);
#endif
}

PathStr pathFromUTF8(string utf8) {
#ifdef _WIN32
    try {
        wstring_convert<codecvt_utf8<wchar_t>> conv;
        return conv.from_bytes(utf8);
    } catch(const range_error&) {
        PANIC("Could not convert UTF-8 to path");
        return L"";
    }
#else
    return move(utf8);
#endif
}

#define API_CALLBACK_HANDLE_EXCEPTIONS_START try {
#define API_CALLBACK_HANDLE_EXCEPTIONS_END \
    } catch(const exception& e) { \
        PANIC("Unhandled exception traversing the vice plugin API: ", e.what()); \
        abort(); \
    } catch(...) { \
        PANIC("Unhandled exception traversing the vice plugin API"); \
        abort(); \
    }

void logCallback(
    void* filenamePtr,
    VicePluginAPI_LogLevel logLevel,
    const char* location,
    const char* msg
) {
API_CALLBACK_HANDLE_EXCEPTIONS_START

    const string& filename = *(string*)filenamePtr;

    const char* logLevelStr;
    if(logLevel == VICE_PLUGIN_API_LOG_LEVEL_ERROR) {
        logLevelStr = "ERROR";
    } else if(logLevel == VICE_PLUGIN_API_LOG_LEVEL_WARNING) {
        logLevelStr = "WARNING";
    } else {
        if(logLevel != VICE_PLUGIN_API_LOG_LEVEL_INFO) {
            WARNING_LOG(
                "Incoming log message from vice plugin ", filename,
                "with unknown log level, defaulting to INFO"
            );
        }
        logLevelStr = "INFO";
    }

    LogWriter(
        logLevelStr,
        filename + " " + location
    )(msg);

API_CALLBACK_HANDLE_EXCEPTIONS_END
}

void panicCallback(void* filenamePtr, const char* location, const char* msg) {
API_CALLBACK_HANDLE_EXCEPTIONS_START

    const string& filename = *(string*)filenamePtr;
    Panicker(filename + " " + location)(msg);

API_CALLBACK_HANDLE_EXCEPTIONS_END
}

void destructorCallback(void* filenamePtr) {
API_CALLBACK_HANDLE_EXCEPTIONS_START

    string* filename = (string*)filenamePtr;
    delete filename;

API_CALLBACK_HANDLE_EXCEPTIONS_END
}

#ifdef _WIN32
string getLastErrorString() {
    LPSTR errBuf;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        GetLastError(),
        0,
        (LPSTR)&errBuf,
        0,
        nullptr
    );
    if(size <= 0) {
        return "Unknown error";
    }

    string err(errBuf, (size_t)size);
    LocalFree(errBuf);
    return err;
}
#endif

}

shared_ptr<VicePlugin> VicePlugin::load(string filename) {
    REQUIRE_UI_THREAD();

#ifdef _WIN32
    void* lib = LoadLibraryA(filename.c_str());
#else
    void* lib = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
#endif
    if(lib == nullptr) {
#ifdef _WIN32
        string err = getLastErrorString();
#else
        const char* rawErr = dlerror();
        string err = rawErr != nullptr ? rawErr : "Unknown error";
#endif
        ERROR_LOG("Loading vice plugin library '", filename, "' failed: ", err);
        return {};
    }

    unique_ptr<APIFuncs> apiFuncs = make_unique<APIFuncs>();

#ifdef _WIN32
    FARPROC sym;

#define LOAD_API_FUNC(name) \
    sym = GetProcAddress((HMODULE)lib, "vicePluginAPI_" #name); \
    if(sym == nullptr) { \
        string err = getLastErrorString(); \
        ERROR_LOG( \
            "Loading symbol 'vicePluginAPI_" #name "' from vice plugin ", \
            filename, " failed: ", err \
        ); \
        REQUIRE(FreeLibrary((HMODULE)lib) != 0); \
        return {}; \
    } \
    apiFuncs->name = (decltype(apiFuncs->name))sym;
#else
    void* sym;

#define LOAD_API_FUNC(name) \
    sym = dlsym(lib, "vicePluginAPI_" #name); \
    if(sym == nullptr) { \
        const char* err = dlerror(); \
        ERROR_LOG( \
            "Loading symbol 'vicePluginAPI_" #name "' from vice plugin ", \
            filename, " failed: ", err != nullptr ? err : "Unknown error" \
        ); \
        REQUIRE(dlclose(lib) == 0); \
        return {}; \
    } \
    apiFuncs->name = (decltype(apiFuncs->name))sym;
#endif

#define FOREACH_VICE_API_FUNC_ITEM(name) LOAD_API_FUNC(name)
    FOREACH_REQUIRED_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM

    const uint64_t APIVersion = 2000000;

    if(!apiFuncs->isAPIVersionSupported(APIVersion)) {
        ERROR_LOG("Vice plugin ", filename, " does not support API version ", APIVersion);
#ifdef _WIN32
        REQUIRE(FreeLibrary((HMODULE)lib) != 0);
#else
        REQUIRE(dlclose(lib) == 0);
#endif
        return {};
    }

    apiFuncs->setGlobalLogCallback(
        APIVersion,
        logCallback,
        new string(filename),
        destructorCallback
    );
    apiFuncs->setGlobalPanicCallback(
        APIVersion,
        panicCallback,
        new string(filename),
        destructorCallback
    );

    if (apiFuncs->isExtensionSupported(APIVersion, "URINavigation")) {
        LOAD_API_FUNC(URINavigation_enable);
    }
    if(apiFuncs->isExtensionSupported(APIVersion, "PluginNavigationControlSupportQuery")) {
        LOAD_API_FUNC(PluginNavigationControlSupportQuery_query);
    }
    if(apiFuncs->isExtensionSupported(APIVersion, "WindowTitle")) {
        LOAD_API_FUNC(WindowTitle_enable);
        LOAD_API_FUNC(WindowTitle_notifyWindowTitleChanged);
    }
    if(apiFuncs->isExtensionSupported(APIVersion, "ZoomInput")) {
        LOAD_API_FUNC(ZoomInput_enable);
    }
    if(apiFuncs->isExtensionSupported(APIVersion, "VirtualKeyboardModeUpdate")) {
        LOAD_API_FUNC(VirtualKeyboardModeUpdate_update);
    }

    return VicePlugin::create(
        CKey(),
        filename,
        lib,
        APIVersion,
        move(apiFuncs)
    );
}

VicePlugin::VicePlugin(CKey, CKey,
    string filename,
    void* lib,
    uint64_t apiVersion,
    unique_ptr<APIFuncs> apiFuncs
) {
    filename_ = filename;
    lib_ = lib;
    apiVersion_ = apiVersion;
    apiFuncs_ = move(apiFuncs);
}

VicePlugin::~VicePlugin() {
#ifdef _WIN32
    REQUIRE(FreeLibrary((HMODULE)lib_) != 0);
#else
    REQUIRE(dlclose(lib_) == 0);
#endif
}

string VicePlugin::getVersionString() {
    REQUIRE_UI_THREAD();

    char* raw = apiFuncs_->createVersionString();
    string val = raw;
    apiFuncs_->free(raw);
    return val;
}

string VicePlugin::getCreditsString() {
    REQUIRE_UI_THREAD();

    char* raw = apiFuncs_->createCreditsString();
    string val = raw;
    apiFuncs_->free(raw);
    return val;
}

vector<VicePlugin::OptionDocsItem> VicePlugin::getOptionDocs() {
    REQUIRE_UI_THREAD();

    vector<VicePlugin::OptionDocsItem> ret;

    apiFuncs_->getOptionDocs(
        apiVersion_,
        [](
            void* data,
            const char* name,
            const char* valSpec,
            const char* desc,
            const char* defaultValStr
        ) {
        API_CALLBACK_HANDLE_EXCEPTIONS_START

            vector<VicePlugin::OptionDocsItem>& ret =
                *(vector<VicePlugin::OptionDocsItem>*)data;
            ret.push_back({name, valSpec, desc, defaultValStr});

        API_CALLBACK_HANDLE_EXCEPTIONS_END
        },
        (void*)&ret
    );

    return ret;
}

namespace {

string sanitizeFilename(string src) {
    string ret;

    for(char c : src) {
        if(c != '/' && c != '\\' && c != '\0') {
            ret.push_back(c);
        }
    }
    if(ret.size() > (size_t)200) {
        ret.resize((size_t)200);
    }
    if(ret == "." || ret == "..") {
        ret += "_file.bin";
    }
    if(ret.empty()) {
        ret = "file.bin";
    }
    return ret;
}

}

ViceFileUpload::ViceFileUpload(CKey,
    shared_ptr<TempDir> tempDir,
    uint64_t uploadIdx,
    string name,
    PathStr srcPath,
    function<void()> srcCleanup
) {
    REQUIRE_UI_THREAD();

    name = sanitizeFilename(move(name));

    tempDir_ = tempDir;
    srcPath_ = move(srcPath);
    srcCleanup_ = move(srcCleanup);

    linkDir_ = tempDir_->path() + PathSep + toPathStr(uploadIdx);
    linkPath_ = linkDir_ + PathSep + pathFromUTF8(name);

#ifdef _WIN32
    REQUIRE(CreateDirectoryW(linkDir_.c_str(), nullptr));

    // On Windows, symbolic links require special permissions, so we just copy the file
    ifstream inFp;
    inFp.open(srcPath_, ifstream::binary);
    REQUIRE(inFp.good());

    ofstream outFp;
    outFp.open(linkPath_, ofstream::binary);
    REQUIRE(outFp.good());

    const size_t BufSize = 8192;
    char buf[BufSize];
    while(true) {
        inFp.read(buf, BufSize);
        REQUIRE(!inFp.bad());
        size_t count = (size_t)inFp.gcount();
        if(count) {
            outFp.write(buf, count);
            REQUIRE(outFp.good());
        }
        if(inFp.eof()) {
            break;
        }
    }

    outFp.close();
    REQUIRE(outFp.good());
    inFp.close();
#else
    REQUIRE(mkdir(linkDir_.c_str(), 0777) == 0);
    REQUIRE(symlink(srcPath_.c_str(), linkPath_.c_str()) == 0);
#endif
}

ViceFileUpload::~ViceFileUpload() {
#ifdef _WIN32
    if(!DeleteFileW(linkPath_.c_str())) {
        WARNING_LOG("Deleting temporary file ", linkPath_, " failed");
    }
    if(!RemoveDirectoryW(linkDir_.c_str())) {
        WARNING_LOG("Deleting temporary directory ", linkDir_, " failed");
    }
#else
    if(unlink(linkPath_.c_str()) != 0) {
        WARNING_LOG("Unlinking temporary symlink ", linkPath_, " failed");
    }
    if(rmdir(linkDir_.c_str()) != 0) {
        WARNING_LOG("Deleting temporary directory ", linkDir_, " failed");
    }
#endif

    srcCleanup_();
}

PathStr ViceFileUpload::path() {
    return linkPath_;
}

namespace {

thread_local ViceContext* threadActivePumpEventsContext = nullptr;

}

// Convenience macro for passing callbacks to the plugin in ViceContext,
// handling appropriate checks and resolving the callback data back to a
// reference to the ViceContext object
#define CTX_CALLBACK_WITHOUT_PUMPEVENTS_CHECK(ret, argDef, ...) \
    [](void* callbackData, auto ...args) -> ret { \
    API_CALLBACK_HANDLE_EXCEPTIONS_START \
        shared_ptr<ViceContext> self = getContext_(callbackData); \
        return ([&self]argDef -> ret { __VA_ARGS__ })(args...); \
    API_CALLBACK_HANDLE_EXCEPTIONS_END \
    }

#define CTX_CALLBACK(ret, argDef, ...) \
    CTX_CALLBACK_WITHOUT_PUMPEVENTS_CHECK(ret, argDef, { \
        if(threadActivePumpEventsContext != self.get()) { \
            PANIC( \
                "Vice plugin unexpectedly called a callback in a thread that " \
                "is not currently executing vicePluginAPI_pumpEvents" \
            ); \
        } \
        REQUIRE_UI_THREAD(); \
        { __VA_ARGS__ } \
    })

shared_ptr<ViceContext> ViceContext::init(
    shared_ptr<VicePlugin> plugin,
    vector<pair<string, string>> options
) {
    REQUIRE_UI_THREAD();

    vector<const char*> optionNames;
    vector<const char*> optionValues;
    for(const pair<string, string>& opt : options) {
        optionNames.push_back(opt.first.c_str());
        optionValues.push_back(opt.second.c_str());
    }

    char* initErrorMsg = nullptr;
    VicePluginAPI_Context* ctx = plugin->apiFuncs_->initContext(
        plugin->apiVersion_,
        optionNames.data(),
        optionValues.data(),
        options.size(),
        "Browservice",
        &initErrorMsg
    );

    if(ctx == nullptr) {
        REQUIRE(initErrorMsg != nullptr);
        cerr
            << "ERROR: Vice plugin " << plugin->filename_ <<
            " initialization failed: " << initErrorMsg << "\n";
        plugin->apiFuncs_->free(initErrorMsg);
        return {};
    }

    return ViceContext::create(CKey(), plugin, ctx);
}

ViceContext::ViceContext(CKey, CKey,
    shared_ptr<VicePlugin> plugin,
    VicePluginAPI_Context* ctx
) {
    plugin_ = plugin;
    ctx_ = ctx;

    state_ = Pending;
    shutdownPending_ = false;
    pumpEventsInQueue_.store(false);
    shutdownCompleteFlag_.store(false);

    nextWindowHandle_ = 1;

    uploadTempDir_ = TempDir::create();
    nextUploadIdx_ = (uint64_t)1;

    if(plugin->apiFuncs_->PluginNavigationControlSupportQuery_query != nullptr) {
        int value = plugin->apiFuncs_->PluginNavigationControlSupportQuery_query(ctx);
        REQUIRE(value == 0 || value == 1);
        hasNavigationControls_ = (bool)value;
    }
}

ViceContext::~ViceContext() {
    REQUIRE(state_ == Pending || state_ == ShutdownComplete);
    plugin_->apiFuncs_->destroyContext(ctx_);
}

optional<bool> ViceContext::hasNavigationControls() {
    return hasNavigationControls_;
}

void ViceContext::start(shared_ptr<ViceContextEventHandler> eventHandler) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == Pending);
    REQUIRE(eventHandler);

    state_ = Running;
    eventHandler_ = eventHandler;
    self_ = shared_from_this();

    // For release builds, we simply use a raw pointer to this object as the
    // callback data. For debug builds, we leak a weak pointer to this object on
    // purpose so that we can catch misbehaving plugins calling callbacks after
    // shutdown (see function getContext_).
#ifdef NDEBUG
    void* callbackData = (void*)this;
#else
    void* callbackData = (void*)(new weak_ptr<ViceContext>(shared_from_this()));
#endif

    if(plugin_->apiFuncs_->URINavigation_enable != nullptr) {
        VicePluginAPI_URINavigation_Callbacks uriNavigationCallbacks;
        memset(&uriNavigationCallbacks, 0, sizeof(VicePluginAPI_URINavigation_Callbacks));

        uriNavigationCallbacks.createWindowWithURI =
            CTX_CALLBACK(uint64_t, (char** msg, const char* uri), {
                string sanitizedURI = sanitizeUTF8String(uri);
                string reason = "Unknown reason";
                uint64_t window =
                    self->eventHandler_->onViceContextCreateWindowRequest(reason, sanitizedURI);
                if(window) {
                    REQUIRE(self->openWindows_.insert(window).second);
                    return window;
                } else {
                    if(msg != nullptr) {
                        *msg = createMallocString(reason, self->plugin_->apiFuncs_->malloc);
                    }
                    return 0;
                }
            });

        uriNavigationCallbacks.navigateWindowToURI =
            CTX_CALLBACK(void, (uint64_t window, const char* uri), {
                REQUIRE(self->openWindows_.count(window));
                string sanitizedURI = sanitizeUTF8String(uri);
                self->eventHandler_->onViceContextNavigateToURI(window, sanitizedURI);
            });

        plugin_->apiFuncs_->URINavigation_enable(ctx_, uriNavigationCallbacks);
    }

    if(plugin_->apiFuncs_->WindowTitle_enable != nullptr) {
        VicePluginAPI_WindowTitle_Callbacks windowTitleCallbacks;
        memset(&windowTitleCallbacks, 0, sizeof(VicePluginAPI_WindowTitle_Callbacks));

        windowTitleCallbacks.getWindowTitle =
            CTX_CALLBACK(char*, (uint64_t window), {
                REQUIRE(self->openWindows_.count(window));
                string title = self->eventHandler_->onViceContextWindowTitleQuery(window);
                return createMallocString(title, self->plugin_->apiFuncs_->malloc);
            });

        plugin_->apiFuncs_->WindowTitle_enable(ctx_, windowTitleCallbacks);
    }

    if(plugin_->apiFuncs_->ZoomInput_enable != nullptr) {
        INFO_LOG("Initializing ZoomInput plugin");
        VicePluginAPI_ZoomInput_Callbacks zoomInputCallbacks;
        memset(&zoomInputCallbacks, 0, sizeof(VicePluginAPI_ZoomInput_Callbacks));

        zoomInputCallbacks.zoomCommand =
            CTX_CALLBACK(void, (uint64_t window, VicePluginAPI_ZoomInput_Command command), {
                REQUIRE(self->openWindows_.count(window));
                if(command == VICE_PLUGIN_API_ZOOM_INPUT_COMMAND_ZOOM_IN) {
                    self->eventHandler_->onViceContextZoomIn(window);
                } else if(command == VICE_PLUGIN_API_ZOOM_INPUT_COMMAND_ZOOM_OUT) {
                    self->eventHandler_->onViceContextZoomOut(window);
                } else {
                    REQUIRE(command == VICE_PLUGIN_API_ZOOM_INPUT_COMMAND_ZOOM_RESET);
                    self->eventHandler_->onViceContextZoomReset(window);
                }
            });

        plugin_->apiFuncs_->ZoomInput_enable(ctx_, zoomInputCallbacks);
    }

    VicePluginAPI_Callbacks callbacks;
    memset(&callbacks, 0, sizeof(VicePluginAPI_Callbacks));

    callbacks.eventNotify = CTX_CALLBACK_WITHOUT_PUMPEVENTS_CHECK(void, (), {
        REQUIRE(!self->shutdownCompleteFlag_.load());

        if(!self->pumpEventsInQueue_.exchange(true)) {
            postTask(self, &ViceContext::pumpEvents_);
        }
    });

    callbacks.shutdownComplete = CTX_CALLBACK(void, (), {
        self->shutdownComplete_();
    });

    callbacks.createWindow = CTX_CALLBACK(uint64_t, (char** msg), {
        string reason = "Unknown reason";
        uint64_t window =
            self->eventHandler_->onViceContextCreateWindowRequest(reason, {});
        if(window) {
            REQUIRE(self->openWindows_.insert(window).second);
            return window;
        } else {
            if(msg != nullptr) {
                *msg = createMallocString(reason, self->plugin_->apiFuncs_->malloc);
            }
            return 0;
        }
    });

    callbacks.closeWindow = CTX_CALLBACK(void, (uint64_t window), {
        REQUIRE(self->openWindows_.erase(window));
        self->uploadModeWindows_.erase(window);
        self->eventHandler_->onViceContextCloseWindow(window);
    });

    callbacks.resizeWindow = CTX_CALLBACK(void, (
        uint64_t window,
        size_t width,
        size_t height
    ), {
        REQUIRE(self->openWindows_.count(window));
        REQUIRE(width);
        REQUIRE(height);

        self->eventHandler_->onViceContextResizeWindow(
            window,
            (int)min(width, (size_t)16384),
            (int)min(height, (size_t)16384)
        );
    });

    callbacks.fetchWindowImage = CTX_CALLBACK(void, (
        uint64_t window,
        void (*putImageFunc)(
            void* putImageFuncData,
            const uint8_t* image,
            size_t width,
            size_t height,
            size_t pitch
        ),
        void* putImageFuncData
    ), {
        REQUIRE(self->openWindows_.count(window));

        bool putImageCalled = false;
        self->eventHandler_->onViceContextFetchWindowImage(
            window,
            [&](
                const uint8_t* image,
                size_t width,
                size_t height,
                size_t pitch
            ) {
                REQUIRE(!putImageCalled);
                putImageCalled = true;

                REQUIRE(width);
                REQUIRE(height);
                putImageFunc(putImageFuncData, image, width, height, pitch);
            }
        );
        REQUIRE(putImageCalled);
    });

#define FORWARD_INPUT_EVENT(name, Name, args, call) \
    callbacks.name = CTX_CALLBACK(void, args, { \
        REQUIRE(self->openWindows_.count(window)); \
        self->eventHandler_->onViceContext ## Name call; \
    })

    FORWARD_INPUT_EVENT(
        mouseDown, MouseDown,
        (uint64_t window, int x, int y, int button),
        (window, x, y, button)
    );
    FORWARD_INPUT_EVENT(
        mouseUp, MouseUp,
        (uint64_t window, int x, int y, int button),
        (window, x, y, button)
    );
    FORWARD_INPUT_EVENT(
        mouseMove, MouseMove,
        (uint64_t window, int x, int y),
        (window, x, y)
    );
    FORWARD_INPUT_EVENT(
        mouseDoubleClick, MouseDoubleClick,
        (uint64_t window, int x, int y, int button),
        (window, x, y, button)
    );
    FORWARD_INPUT_EVENT(
        mouseWheel, MouseWheel,
        (uint64_t window, int x, int y, int dx, int dy),
        (window, x, y, dx, dy)
    );
    FORWARD_INPUT_EVENT(
        mouseLeave, MouseLeave,
        (uint64_t window, int x, int y),
        (window, x, y)
    );
    FORWARD_INPUT_EVENT(
        keyDown, KeyDown,
        (uint64_t window, int key),
        (window, key)
    );
    FORWARD_INPUT_EVENT(
        keyUp, KeyUp,
        (uint64_t window, int key),
        (window, key)
    );
    FORWARD_INPUT_EVENT(
        loseFocus, LoseFocus,
        (uint64_t window),
        (window)
    );

    callbacks.navigate = CTX_CALLBACK(void, (uint64_t window, int direction), {
        REQUIRE(self->openWindows_.count(window));
        REQUIRE(direction >= -1 && direction <= 1);
        self->eventHandler_->onViceContextNavigate(window, direction);
    });

    callbacks.copyToClipboard = CTX_CALLBACK(void, (const char* text), {
        REQUIRE(text != nullptr);
        string sanitizedText = sanitizeUTF8String(text);
        self->eventHandler_->onViceContextCopyToClipboard(move(sanitizedText));
    });

    callbacks.requestClipboardContent = CTX_CALLBACK(int, (), {
        self->eventHandler_->onViceContextRequestClipboardContent();
        return 1;
    });

    callbacks.uploadFile = CTX_CALLBACK(void, (
        uint64_t window,
        const char* name,
        const char* path,
        void (*cleanup)(void*),
        void* cleanupData
    ), {
        REQUIRE(path != nullptr);
        REQUIRE(cleanup != nullptr);

        REQUIRE(self->openWindows_.count(window));
        REQUIRE(self->uploadModeWindows_.erase(window));

        function<void()> cleanupFunc = [cleanup, cleanupData]() {
            cleanup(cleanupData);
        };
        self->eventHandler_->onViceContextUploadFile(
            window,
            ViceFileUpload::create(
                self->uploadTempDir_,
                self->nextUploadIdx_++,
                name,
                pathFromUTF8(path),
                cleanupFunc
            )
        );
    });

    callbacks.cancelFileUpload = CTX_CALLBACK(void, (uint64_t window), {
        REQUIRE(self->openWindows_.count(window));
        REQUIRE(self->uploadModeWindows_.erase(window));

        self->eventHandler_->onViceContextCancelFileUpload(window);
    });

    plugin_->apiFuncs_->start(
        ctx_,
        callbacks,
        callbackData
    );
}

void ViceContext::shutdown() {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == Running);
    REQUIRE(!shutdownPending_);

    shutdownPending_ = true;
    plugin_->apiFuncs_->shutdown(ctx_);
}

#define RUNNING_CONTEXT_FUNC_CHECKS() \
    do { \
        REQUIRE_UI_THREAD(); \
        REQUIRE(state_ == Running); \
        REQUIRE(threadActivePumpEventsContext == nullptr); \
    } while(false)

bool ViceContext::requestCreatePopup(
    uint64_t parentWindow,
    uint64_t popupWindow,
    string& msg
) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(parentWindow));
    REQUIRE(popupWindow);
    REQUIRE(!openWindows_.count(popupWindow));

    char* msgC = nullptr;

    int result = plugin_->apiFuncs_->createPopupWindow(
        ctx_, parentWindow, popupWindow, &msgC
    );
    REQUIRE(result == 0 || result == 1);

    if(result == 1) {
        REQUIRE(msgC == nullptr);
        REQUIRE(openWindows_.insert(popupWindow).second);
        return true;
    } else {
        REQUIRE(msgC != nullptr);
        msg = msgC;
        plugin_->apiFuncs_->free(msgC);
        return false;
    }
}

void ViceContext::closeWindow(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();

    REQUIRE(openWindows_.erase(window));
    uploadModeWindows_.erase(window);
    plugin_->apiFuncs_->closeWindow(ctx_, window);
}

void ViceContext::notifyWindowViewChanged(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    plugin_->apiFuncs_->notifyWindowViewChanged(ctx_, window);
}

void ViceContext::notifyWindowTitleChanged(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    if(plugin_->apiFuncs_->WindowTitle_enable != nullptr) {
        REQUIRE(plugin_->apiFuncs_->WindowTitle_notifyWindowTitleChanged != nullptr);
        plugin_->apiFuncs_->WindowTitle_notifyWindowTitleChanged(ctx_, window);
    }
}

void ViceContext::setWindowCursor(uint64_t window, int cursor) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    VicePluginAPI_MouseCursor apiCursor;
    if(cursor == HandCursor) {
        apiCursor = VICE_PLUGIN_API_MOUSE_CURSOR_HAND;
    } else if(cursor == TextCursor) {
        apiCursor = VICE_PLUGIN_API_MOUSE_CURSOR_TEXT;
    } else {
        REQUIRE(cursor == NormalCursor);
        apiCursor = VICE_PLUGIN_API_MOUSE_CURSOR_NORMAL;
    }

    plugin_->apiFuncs_->setWindowCursor(ctx_, window, apiCursor);
}

void ViceContext::setWindowVirtualKeyboardMode(uint64_t window, VirtualKeyboardMode mode) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    if(plugin_->apiFuncs_->VirtualKeyboardModeUpdate_update != nullptr) {
        VicePluginAPI_VirtualKeyboardModeUpdate_Mode apiMode;
        switch(mode) {
            case VirtualKeyboardMode::None: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_NONE; break;
            case VirtualKeyboardMode::Default: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_DEFAULT; break;
            case VirtualKeyboardMode::Text: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_TEXT; break;
            case VirtualKeyboardMode::Tel: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_TEL; break;
            case VirtualKeyboardMode::URL: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_URL; break;
            case VirtualKeyboardMode::Email: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_EMAIL; break;
            case VirtualKeyboardMode::Numeric: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_NUMERIC; break;
            case VirtualKeyboardMode::Decimal: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_DECIMAL; break;
            case VirtualKeyboardMode::Search: apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_SEARCH; break;
            default: {
                apiMode = VICE_PLUGIN_API_VIRTUAL_KEYBOARD_MODE_UPDATE_MODE_DEFAULT;
                WARNING_LOG("Unknown virtual keyboard mode, using default");
            } break;
        }
        plugin_->apiFuncs_->VirtualKeyboardModeUpdate_update(ctx_, window, apiMode);
    }
}

optional<pair<vector<string>, size_t>> ViceContext::windowQualitySelectorQuery(
    uint64_t window
) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    char* qualityList = nullptr;
    size_t currentQuality = (size_t)-1;
    int result = plugin_->apiFuncs_->windowQualitySelectorQuery(
        ctx_, window, &qualityList, &currentQuality
    );
    REQUIRE(result == 0 || result == 1);

    if(result) {
        REQUIRE(qualityList != nullptr);
        string labelStr = qualityList;
        plugin_->apiFuncs_->free(qualityList);

        vector<string> labels;
        size_t i = 0;
        while(i < labelStr.size()) {
            size_t j = i;
            while(true) {
                REQUIRE(j < labelStr.size());
                if(labelStr[j] == '\n') {
                    break;
                }
                ++j;
            }
            size_t len = j - i;
            REQUIRE(len >= (size_t)1 && len <= (size_t)3);
            labels.push_back(labelStr.substr(i, len));
            i = j;
            ++i;
        }
        REQUIRE(!labels.empty());
        REQUIRE(currentQuality < labels.size());

        for(const string& label : labels) {
            for(char c : label) {
                REQUIRE((int)c >= 33 && (int)c <= 126);
            }
        }

        return pair<vector<string>, size_t>(labels, currentQuality);
    } else {
        return {};
    }
}

void ViceContext::windowQualityChanged(uint64_t window, size_t idx) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    plugin_->apiFuncs_->windowQualityChanged(ctx_, window, idx);
}

bool ViceContext::windowNeedsClipboardButtonQuery(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    int result =
        plugin_->apiFuncs_->windowNeedsClipboardButtonQuery(ctx_, window);
    REQUIRE(result == 0 || result == 1);
    return (bool)result;
}

void ViceContext::windowClipboardButtonPressed(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    plugin_->apiFuncs_->windowClipboardButtonPressed(ctx_, window);
}

void ViceContext::putClipboardContent(string text) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    plugin_->apiFuncs_->putClipboardContent(ctx_, text.c_str());
}

void ViceContext::putFileDownload(
    uint64_t window, shared_ptr<CompletedDownload> file
) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));

    string name = file->name();
    PathStr path = file->path();
    string pathUTF8 = pathToUTF8(path);

    plugin_->apiFuncs_->putFileDownload(
        ctx_,
        window,
        name.c_str(),
        pathUTF8.c_str(),
        [](void* cleanupData) {
            REQUIRE(cleanupData != nullptr);
            shared_ptr<CompletedDownload>* file =
                (shared_ptr<CompletedDownload>*)cleanupData;
            delete file;
        },
        (void*)new shared_ptr<CompletedDownload>(file)
    );
}

bool ViceContext::startFileUpload(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));
    REQUIRE(!uploadModeWindows_.count(window));

    int result = plugin_->apiFuncs_->startFileUpload(ctx_, window);
    REQUIRE(result == 0 || result == 1);

    if(result == 1) {
        REQUIRE(uploadModeWindows_.insert(window).second);
    }

    return (bool)result;
}

void ViceContext::cancelFileUpload(uint64_t window) {
    RUNNING_CONTEXT_FUNC_CHECKS();
    REQUIRE(openWindows_.count(window));
    REQUIRE(uploadModeWindows_.erase(window));

    plugin_->apiFuncs_->cancelFileUpload(ctx_, window);
}

shared_ptr<ViceContext> ViceContext::getContext_(void* callbackData) {
    if(callbackData == nullptr) {
        PANIC("Vice plugin sent unexpected NULL pointer as callback data");
    }

#ifdef NDEBUG
    return ((ViceContext*)callbackData)->shared_from_this();
#else
    if(shared_ptr<ViceContext> self = ((weak_ptr<ViceContext>*)callbackData)->lock()) {
        if(self->state_ == ShutdownComplete) {
            PANIC("Vice plugin called a callback for a context that has shut down");
        }
        REQUIRE(self->state_ == Running);
        return self;
    } else {
        PANIC("Vice plugin called a callback for a context that has been destroyed");
        abort();
    }
#endif
}

void ViceContext::pumpEvents_() {
    REQUIRE_UI_THREAD();
    REQUIRE(threadActivePumpEventsContext == nullptr);

    if(state_ != Running) {
        return;
    }

    pumpEventsInQueue_.store(false);

    threadActivePumpEventsContext = this;
    plugin_->apiFuncs_->pumpEvents(ctx_);
    threadActivePumpEventsContext = nullptr;
}

void ViceContext::shutdownComplete_() {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == Running);
    REQUIRE(shutdownPending_);

    state_ = ShutdownComplete;
    shutdownPending_ = false;
    shutdownCompleteFlag_.store(true);
    self_.reset();

    REQUIRE(eventHandler_);
    postTask(
        eventHandler_,
        &ViceContextEventHandler::onViceContextShutdownComplete
    );
    eventHandler_.reset();
}

}
