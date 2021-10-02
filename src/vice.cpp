#include "vice.hpp"

#include "download_manager.hpp"
#include "globals.hpp"
#include "temp_dir.hpp"
#include "widget.hpp"

#include "../vice_plugin_api.h"

/*
#include <dlfcn.h>
#include <unistd.h>
*/
#include <sys/stat.h>
#include <sys/types.h>

namespace browservice {

struct VicePlugin::APIFuncs {
#define FOREACH_REQUIRED_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isAPIVersionSupported) \
    FOREACH_VICE_API_FUNC_ITEM(getVersionString) \
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
    FOREACH_VICE_API_FUNC_ITEM(setGlobalPanicCallback)

#define FOREACH_VICE_API_FUNC \
    FOREACH_REQUIRED_VICE_API_FUNC \
    FOREACH_VICE_API_FUNC_ITEM(isExtensionSupported) \
    FOREACH_VICE_API_FUNC_ITEM(URINavigation_enable)

#define FOREACH_VICE_API_FUNC_ITEM(name) \
    decltype(&vicePluginAPI_ ## name) name = nullptr;

    FOREACH_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM
};

namespace {

char* createMallocString(string val) {
    size_t size = val.size() + 1;
    char* ret = (char*)malloc(size);
    REQUIRE(ret != nullptr);
    memcpy(ret, val.c_str(), size);
    return ret;
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

}

shared_ptr<VicePlugin> VicePlugin::load(string filename) {
    REQUIRE_UI_THREAD();

    void* lib = nullptr;// dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if(lib == nullptr) {
        const char* err = "NOT IMPLEMENTED";// dlerror();
        ERROR_LOG(
            "Loading vice plugin library '", filename,
            "' failed: ", err != nullptr ? err : "Unknown error"
        );
        return {};
    }

    unique_ptr<APIFuncs> apiFuncs = make_unique<APIFuncs>();

    void* sym;

#define LOAD_API_FUNC(name) \
    sym = nullptr; /*dlsym(lib, "vicePluginAPI_" #name);*/ \
    if(sym == nullptr) { \
        const char* err = "NOT IMPLEMENTED";/*dlerror();*/ \
        ERROR_LOG( \
            "Loading symbol 'vicePluginAPI_" #name "' from vice plugin ", \
            filename, " failed: ", err != nullptr ? err : "Unknown error" \
        ); \
        /*REQUIRE(dlclose(lib) == 0);*/ \
        return {}; \
    } \
    apiFuncs->name = (decltype(apiFuncs->name))sym;

#define FOREACH_VICE_API_FUNC_ITEM(name) LOAD_API_FUNC(name)
    FOREACH_REQUIRED_VICE_API_FUNC
#undef FOREACH_VICE_API_FUNC_ITEM

    const uint64_t BasicAPIVersion = 1000000;
    const uint64_t ExtensionAPIVersion = 1000001;

    uint64_t apiVersion;
    if(apiFuncs->isAPIVersionSupported(ExtensionAPIVersion)) {
        apiVersion = ExtensionAPIVersion;
        LOAD_API_FUNC(isExtensionSupported);
        if(apiFuncs->isExtensionSupported(apiVersion, "URINavigation")) {
            LOAD_API_FUNC(URINavigation_enable);
        }
    } else {
        apiVersion = BasicAPIVersion;
        if(!apiFuncs->isAPIVersionSupported(apiVersion)) {
            ERROR_LOG(
                "Vice plugin ", filename,
                " does not support API version ", apiVersion
            );
            //REQUIRE(dlclose(lib) == 0);
            return {};
        }
    }

    apiFuncs->setGlobalLogCallback(
        apiVersion,
        logCallback,
        new string(filename),
        destructorCallback
    );
    apiFuncs->setGlobalPanicCallback(
        apiVersion,
        panicCallback,
        new string(filename),
        destructorCallback
    );

    return VicePlugin::create(
        CKey(),
        filename,
        lib,
        apiVersion,
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
    //REQUIRE(dlclose(lib_) == 0);
}

string VicePlugin::getVersionString() {
    REQUIRE_UI_THREAD();

    char* raw = apiFuncs_->getVersionString();
    string val = raw;
    free(raw);
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
    string srcPath,
    function<void()> srcCleanup
) {
    REQUIRE_UI_THREAD();
/*
    name = sanitizeFilename(move(name));

    tempDir_ = tempDir;
    srcPath_ = move(srcPath);
    srcCleanup_ = move(srcCleanup);

    linkDir_ = tempDir_->path() + "/" + toString(uploadIdx);
    linkPath_ = linkDir_ + "/" + name;

    REQUIRE(mkdir(linkDir_.c_str(), 0777) == 0);
    REQUIRE(symlink(srcPath_.c_str(), linkPath_.c_str()) == 0);
*/
}

ViceFileUpload::~ViceFileUpload() {
/*
    if(unlink(linkPath_.c_str()) != 0) {
        WARNING_LOG("Unlinking temporary symlink ", linkPath_, " failed");
    }
    if(rmdir(linkDir_.c_str()) != 0) {
        WARNING_LOG("Deleting temporary directory ", linkDir_, " failed");
    }

    srcCleanup_();
*/
}

string ViceFileUpload::path() {
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
        free(initErrorMsg);
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

//    uploadTempDir_ = TempDir::create();
    nextUploadIdx_ = (uint64_t)1;
}

ViceContext::~ViceContext() {
    REQUIRE(state_ == Pending || state_ == ShutdownComplete);
    plugin_->apiFuncs_->destroyContext(ctx_);
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
                        *msg = createMallocString(reason);
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
                *msg = createMallocString(reason);
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
                path,
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
        free(msgC);
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
        free(qualityList);

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
    string path = file->path();

    plugin_->apiFuncs_->putFileDownload(
        ctx_,
        window,
        name.c_str(),
        path.c_str(),
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
