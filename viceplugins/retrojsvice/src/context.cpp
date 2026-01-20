#include "context.hpp"

#include "download.hpp"
#include "html.hpp"
#include "secrets.hpp"
#include "upload.hpp"

namespace retrojsvice {

namespace {

const string defaultHTTPListenAddr = "0.0.0.0:8080";
const int defaultHTTPMaxThreads = 100;

set<string> trueValues = {"1", "yes", "true", "enable", "enabled"};
set<string> falseValues = {"0", "no", "false", "disable", "disabled"};

// Returns (true, value) or (false, error message).
pair<bool, string> parseHTTPAuthOption(string optValue) {
    if(optValue.empty()) {
        return make_pair(true, string());
    } else {
        string value;
        if(optValue == "env") {
            const char* valuePtr = getenv("HTTP_AUTH_CREDENTIALS");
            if(valuePtr == nullptr) {
                return make_pair(
                    false,
                    "Option http-auth set to 'env' but environment "
                    "variable HTTP_AUTH_CREDENTIALS is missing"
                );
            }
            value = valuePtr;
        } else {
            value = optValue;
        }

        size_t colonPos = value.find(':');
        if(colonPos == value.npos || !colonPos || colonPos + 1 == value.size()) {
            return make_pair(false, "Invalid value for option http-auth");
        }
        return make_pair(true, value);
    }
}

string sanitizeProgramName(string src) {
    string ret;
    for(char c : src) {
        if(
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == ' '
        ) {
            if(ret.size() >= 60) {
                ret.append("...");
                break;
            }
            ret.push_back(c);
        }
    }
    if(ret.empty()) {
        ret = "retrojsvice";
    }
    return ret;
}

string htmlEscapeString(string src) {
    string ret;
    for(char c : src) {
        if(c == '&') {
            ret += "&amp;";
        } else if(c == '<') {
            ret += "&lt;";
        } else if(c == '>') {
            ret += "&gt;";
        } else if(c == '"') {
            ret += "&quot;";
        } else if(c == '\'') {
            ret += "&apos;";
        } else {
            ret.push_back(c);
        }
    }
    return ret;
}

string pathToUTF8(PathStr path) {
#ifdef _WIN32
    try {
        wstring_convert<codecvt_utf8<wchar_t>> conv;
        return conv.to_bytes(path);
    }
    catch(const range_error&) {
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
    }
    catch(const range_error&) {
        PANIC("Could not convert UTF-8 to path");
        return L"";
    }
#else
    return move(utf8);
#endif
}

thread_local bool threadRunningPumpEvents = false;

}

class Context::APILock {
public:
    APILock(Context* ctx) {
        REQUIRE(ctx != nullptr);
        ctx_ = ctx;

        if(ctx_->inAPICall_.exchange(true)) {
            PANIC(
                "Two API calls concerning the same context running concurrently"
            );
        }

        if(inAPIThread_) {
            PANIC(
                "Plugin API call made while another API call is running in the "
                "same thread"
            );
        }
        inAPIThread_ = true;
    }
    APILock(APILock&& src) {
        REQUIRE(src.ctx_ != nullptr);
        ctx_ = src.ctx_;
        src.ctx_ = nullptr;
    }
    ~APILock() {
        if(ctx_ != nullptr) {
            REQUIRE(inAPIThread_);
            inAPIThread_ = false;

            REQUIRE(ctx_->inAPICall_.exchange(false));
        }
    }

    APILock(const APILock&);
    APILock& operator=(const APILock&);
    APILock& operator=(APILock&&);

private:
    Context* ctx_;

    friend class Context::RunningAPILock;
};

class Context::RunningAPILock {
public:
    RunningAPILock(Context* ctx)
        : RunningAPILock(APILock(ctx))
    {}
    RunningAPILock(APILock apiLock)
        : apiLock_(move(apiLock))
    {
        Context* ctx = apiLock_.ctx_;

        if(ctx->state_ == Pending) {
            PANIC("Unexpected API call for context that has not been started");
        }
        if(ctx->state_ == ShutdownComplete) {
            PANIC("Unexpected API call for context that has already been shut down");
        }
        REQUIRE(ctx->state_ == Running);

        REQUIRE(ctx->taskQueue_);
        activeTaskQueueLock_.emplace(ctx->taskQueue_);
    }

    DISABLE_COPY_MOVE(RunningAPILock);

private:
    APILock apiLock_;
    optional<ActiveTaskQueueLock> activeTaskQueueLock_;
};

variant<shared_ptr<Context>, string> Context::init(
    vector<pair<string, string>> options,
    string programName
) {
    int defaultQuality = 101;
    SocketAddress httpListenAddr =
        SocketAddress::parse(defaultHTTPListenAddr).value();
    int httpMaxThreads = defaultHTTPMaxThreads;
    string httpAuthCredentials;
    bool allowQualitySelector = true;
    bool setupNavigationForwarding = true;

    for(const pair<string, string>& option : options) {
        const string& name = option.first;
        const string& value = option.second;

        if(name == "default-quality") {
            string lowValue = value;
            for(char& c : lowValue) {
                c = tolower(c);
            }
            if(lowValue == "png") {
                defaultQuality = 101;
            } else {
                optional<int> parsed = parseString<int>(value);
                if(!parsed.has_value() || *parsed < 10 || *parsed > 100) {
                    return "Invalid value '" + value + "' for option default-quality";
                }
                defaultQuality = *parsed;
            }
        } else if(name == "http-listen-addr") {
            optional<SocketAddress> parsed = SocketAddress::parse(value);
            if(!parsed.has_value()) {
                return "Invalid value '" + value + "' for option http-listen-addr";
            }
            httpListenAddr = *parsed;
        } else if(name == "http-max-threads") {
            optional<int> parsed = parseString<int>(value);
            if(!parsed.has_value() || *parsed <= 0) {
                return "Invalid value '" + value + "' for option http-max-threads";
            }
            httpMaxThreads = *parsed;
        } else if(name == "http-auth") {
            pair<bool, string> result = parseHTTPAuthOption(value);
            if(result.first) {
                httpAuthCredentials = result.second;
            } else {
                return result.second;
            }
        } else if(name == "quality-selector") {
            string lowValue = value;
            for(char& c : lowValue) {
                c = tolower(c);
            }
            if(trueValues.count(lowValue)) {
                allowQualitySelector = true;
            } else if(falseValues.count(lowValue)) {
                allowQualitySelector = false;
            } else {
                return "Invalid value '" + value + "' for option quality-selector";
            }
        } else if(name == "navigation-forwarding") {
            string lowValue = value;
            for(char& c : lowValue) {
                c = tolower(c);
            }
            if(trueValues.count(lowValue)) {
                setupNavigationForwarding = true;
            } else if(falseValues.count(lowValue)) {
                setupNavigationForwarding = false;
            } else {
                return "Invalid value '" + value + "' for option navigation-forwarding";
            }
        } else {
            return "Unrecognized option '" + name + "'";
        }
    }

    return Context::create(
        CKey(),
        defaultQuality,
        httpListenAddr,
        httpMaxThreads,
        httpAuthCredentials,
        allowQualitySelector,
        setupNavigationForwarding,
        programName
    );
}

Context::Context(CKey, CKey,
    int defaultQuality,
    SocketAddress httpListenAddr,
    int httpMaxThreads,
    string httpAuthCredentials,
    bool allowQualitySelector,
    bool setupNavigationForwarding,
    string programName
)
    : httpListenAddr_(httpListenAddr)
{
    INFO_LOG("Creating retrojsvice plugin context");

    defaultQuality_ = defaultQuality;
    httpMaxThreads_ = httpMaxThreads;
    httpAuthCredentials_ = httpAuthCredentials;
    allowQualitySelector_ = allowQualitySelector;
    setupNavigationForwarding_ = setupNavigationForwarding;
    programName_ = sanitizeProgramName(programName);

    state_ = Pending;
    shutdownPhase_ = NoPendingShutdown;
    inAPICall_.store(false);

    memset(&callbacks_, 0, sizeof(VicePluginAPI_Callbacks));
    callbackData_ = nullptr;
}

Context::~Context() {
    APILock apiLock(this);

    if(state_ == Running) {
        PANIC("Destroying a plugin context that is still running");
    }
    REQUIRE(state_ == Pending || state_ == ShutdownComplete);

    INFO_LOG("Destroying retrojsvice plugin context");
}

void Context::URINavigation_enable(VicePluginAPI_URINavigation_Callbacks callbacks) {
    APILock apiLock(this);

    REQUIRE(state_ == Pending);

    REQUIRE(!uriNavigationCallbacks_.has_value());
    uriNavigationCallbacks_ = callbacks;
}

int Context::PluginNavigationControlSupportQuery_query() {
    APILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    return 1;
}

void Context::start(
    VicePluginAPI_Callbacks callbacks,
    void* callbackData
) {
    APILock apiLock(this);

    if(state_ == Running) {
        PANIC("Starting a plugin context that is already running");
    }
    if(state_ == ShutdownComplete) {
        PANIC("Starting a plugin that has already been shut down");
    }
    REQUIRE(state_ == Pending);

    INFO_LOG("Starting plugin");

    callbacks_ = callbacks;
    callbackData_ = callbackData;

    state_ = Running;
    taskQueue_ = TaskQueue::create(shared_from_this());

    RunningAPILock runningApiLock(move(apiLock));

    httpServer_ = HTTPServer::create(
        shared_from_this(),
        httpListenAddr_,
        httpMaxThreads_
    );
    secretGen_ = SecretGenerator::create();
    windowManager_ = WindowManager::create(
        shared_from_this(),
        secretGen_,
        programName_,
        defaultQuality_,
        setupNavigationForwarding_
    );

    clipboardCSRFToken_ = secretGen_->generateCSRFToken();
}

void Context::shutdown() {
    RunningAPILock apiLock(this);

    if(shutdownPhase_ != NoPendingShutdown) {
        PANIC("Requested shutdown of a plugin that is already shutting down");
    }

    if(clipboardTimeout_) {
        clipboardTimeout_->expedite();
        clipboardTimeout_.reset();
    }

    INFO_LOG("Shutting down plugin");

    shutdownPhase_ = WaitWindowManager;

    shared_ptr<Context> self = shared_from_this();
    postTask([self]() {
        REQUIRE(self->shutdownPhase_ == WaitWindowManager);

        REQUIRE(self->windowManager_);

        // Will only result in onWindowManagerCloseWindow events
        self->windowManager_->close(mce);

        self->shutdownPhase_ = WaitHTTPServer;

        REQUIRE(self->httpServer_);
        self->httpServer_->shutdown();
    });
}

void Context::pumpEvents() {
    RunningAPILock apiLock(this);

    REQUIRE(!threadRunningPumpEvents);
    threadRunningPumpEvents = true;

    taskQueue_->runTasks(mce);

    threadRunningPumpEvents = false;
}

int Context::createPopupWindow(
    uint64_t parentWindow,
    uint64_t popupWindow,
    char** msg
) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    string reason = "Unknown reason";
    if(windowManager_->createPopupWindow(parentWindow, popupWindow, reason)) {
        return 1;
    } else {
        if(msg != nullptr) {
            *msg = createMallocString(reason);
        }
        return 0;
    }
}

void Context::closeWindow(uint64_t window) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    windowManager_->closeWindow(window);
}

void Context::notifyWindowViewChanged(uint64_t window) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    windowManager_->notifyViewChanged(window);
}

void Context::setWindowCursor(
    uint64_t window,
    VicePluginAPI_MouseCursor cursor
) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    int cursorSignal;
    if(cursor == VICE_PLUGIN_API_MOUSE_CURSOR_NORMAL) {
        cursorSignal = ImageCompressor::CursorSignalNormal;
    } else if(cursor == VICE_PLUGIN_API_MOUSE_CURSOR_HAND) {
        cursorSignal = ImageCompressor::CursorSignalHand;
    } else {
        REQUIRE(cursor == VICE_PLUGIN_API_MOUSE_CURSOR_TEXT);
        cursorSignal = ImageCompressor::CursorSignalText;
    }

    windowManager_->setCursor(window, cursorSignal);
}

int Context::windowQualitySelectorQuery(
    uint64_t window,
    char** qualityListOut,
    size_t* currentQualityOut
) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);
    REQUIRE(qualityListOut != nullptr);
    REQUIRE(currentQualityOut != nullptr);

    if(!allowQualitySelector_) {
        return 0;
    }

    optional<pair<vector<string>, int>> result =
        windowManager_->qualitySelectorQuery(window);

    if(result) {
        const vector<string>& labels = result->first;
        size_t currentValue = result->second;

        REQUIRE(!labels.empty());
        string labelList;
        for(const string& label : labels) {
            REQUIRE(label.size() >= (size_t)1 && label.size() <= (size_t)3);
            for(char c : label) {
                REQUIRE((int)c >= 33 && (int)c <= 126);
            }
            labelList += label;
            labelList.push_back('\n');
        }

        REQUIRE(currentValue < labels.size());

        *qualityListOut = createMallocString(labelList);
        *currentQualityOut = currentValue;

        return 1;
    } else {
        return 0;
    }
}

void Context::windowQualityChanged(uint64_t window, size_t qualityIdx) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);
    REQUIRE(allowQualitySelector_);

    windowManager_->qualityChanged(window, qualityIdx);
}

int Context::windowNeedsClipboardButtonQuery(uint64_t window) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    return (int)windowManager_->needsClipboardButtonQuery(window);
}

void Context::windowClipboardButtonPressed(uint64_t window) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    windowManager_->clipboardButtonPressed(window);
}

void Context::putClipboardContent(const char* text) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    string sanitizedText = sanitizeUTF8String(text);

    vector<shared_ptr<HTTPRequest>> requests;
    swap(clipboardRequests_, requests);
    for(shared_ptr<HTTPRequest> request : requests) {
        request->sendHTMLResponse(
            200,
            writeClipboardHTML,
            {programName_, htmlEscapeString(sanitizedText), clipboardCSRFToken_}
        );
    }
}

void Context::putFileDownload(
    uint64_t window,
    const char* name,
    const char* path,
    void (*cleanup)(void*),
    void* cleanupData
) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    function<void()> cleanupFunc = [cleanup, cleanupData]() {
        cleanup(cleanupData);
    };
    shared_ptr<FileDownload> file =
        FileDownload::create(name, pathFromUTF8(path), cleanupFunc);

    windowManager_->putFileDownload(window, file);
}

int Context::startFileUpload(uint64_t window) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    return (int)windowManager_->startFileUpload(window);
}

void Context::cancelFileUpload(uint64_t window) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    windowManager_->cancelFileUpload(window);
}

vector<tuple<string, string, string, string>> Context::getOptionDocs() {
    vector<tuple<string, string, string, string>> ret;

    ret.emplace_back(
        "default-quality",
        "QUALITY",
        "initial image quality for each window (10..100 or PNG)",
        "default: PNG"
    );
    ret.emplace_back(
        "http-listen-addr",
        "IP:PORT",
        "bind address and port for the HTTP server",
        "default: " + defaultHTTPListenAddr
    );
    ret.emplace_back(
        "http-max-threads",
        "COUNT",
        "maximum number of HTTP server threads",
        "default: " + toString(defaultHTTPMaxThreads)
    );
    ret.emplace_back(
        "http-auth",
        "USER:PASSWORD",
        "if nonempty, the client is required to authenticate using "
        "HTTP basic authentication with given username and "
        "password; if the special value 'env' is specified, the "
        "value is read from the environment variable "
        "HTTP_AUTH_CREDENTIALS",
        "default empty"
    );
    ret.emplace_back(
        "quality-selector",
        "YES/NO",
        "make image quality adjustable using a quality selector widget",
        "default: yes"
    );
    ret.emplace_back(
        "navigation-forwarding",
        "YES/NO",
        "forward client back/forward buttons to the program; "
        "may have compatibility issues with some clients",
        "default: yes"
    );

    return ret;
}

void Context::onHTTPServerRequest(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();
    REQUIRE(state_ == Running);

    if(!httpAuthCredentials_.empty()) {
        optional<string> reqCred = request->getBasicAuthCredentials();
        if(!reqCred || !passwordsEqual(*reqCred, httpAuthCredentials_)) {
            request->sendTextResponse(
                401,
                "Unauthorized",
                true,
                {{
                    "WWW-Authenticate",
                    "Basic realm=\"Restricted\", charset=\"UTF-8\""
                }}
            );
            return;
        }
    }

    if(shutdownPhase_ != NoPendingShutdown) {
        request->sendTextResponse(503, "ERROR: Service is shutting down\n");
        return;
    }

    if(request->path() == "/clipboard/") {
        handleClipboardHTTPRequest_(mce, request);
    } else {
        windowManager_->handleHTTPRequest(mce, request);
    }
}

void Context::onHTTPServerShutdownComplete() {
    REQUIRE_API_THREAD();
    REQUIRE(state_ == Running);
    REQUIRE(shutdownPhase_ == WaitHTTPServer);

    shutdownPhase_ = WaitTaskQueue;

    REQUIRE(taskQueue_);
    taskQueue_->shutdown();
}

void Context::onTaskQueueNeedsRunTasks() {
    REQUIRE(callbacks_.eventNotify != nullptr);
    callbacks_.eventNotify(callbackData_);
}

void Context::onTaskQueueShutdownComplete() {
    REQUIRE_API_THREAD();
    REQUIRE(state_ == Running);
    REQUIRE(shutdownPhase_ == WaitTaskQueue);

    state_ = ShutdownComplete;
    shutdownPhase_ = NoPendingShutdown;

    INFO_LOG("Plugin shutdown complete");

    REQUIRE(callbacks_.shutdownComplete != nullptr);
    callbacks_.shutdownComplete(callbackData_);

    memset(&callbacks_, 0, sizeof(VicePluginAPI_Callbacks));
}

variant<uint64_t, string> Context::onWindowManagerCreateWindowRequest() {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);

    REQUIRE(callbacks_.createWindow != nullptr);
    char* msgC = nullptr;
    uint64_t handle = callbacks_.createWindow(callbackData_, &msgC);

    if(handle) {
        REQUIRE(msgC == nullptr);
        return handle;
    } else {
        REQUIRE(msgC != nullptr);
        string msg = msgC;
        free(msgC);
        return msg;
    }
}

variant<uint64_t, string> Context::onWindowManagerCreateWindowWithURIRequest(string uri) {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);

    if(!uriNavigationCallbacks_) {
        return string("Program has not enabled URINavigation vice plugin API extension");
    }

    REQUIRE(uriNavigationCallbacks_->createWindowWithURI != nullptr);
    char* msgC = nullptr;
    uint64_t handle = uriNavigationCallbacks_->createWindowWithURI(callbackData_, &msgC, uri.c_str());

    if(handle) {
        REQUIRE(msgC == nullptr);
        return handle;
    } else {
        REQUIRE(msgC != nullptr);
        string msg = msgC;
        free(msgC);
        return msg;
    }
}

void Context::onWindowManagerCloseWindow(uint64_t window) {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);
    REQUIRE(window);

    REQUIRE(callbacks_.closeWindow != nullptr);
    callbacks_.closeWindow(callbackData_, window);
}

void Context::onWindowManagerFetchImage(
    uint64_t window,
    function<void(const uint8_t*, size_t, size_t, size_t)> func
) {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);
    REQUIRE(window);

    auto callFunc = [](
        void* funcPtr,
        const uint8_t* image,
        size_t width,
        size_t height,
        size_t pitch
    ) {
        REQUIRE(funcPtr != nullptr);
        function<void(const uint8_t*, size_t, size_t, size_t)>& func =
            *(function<void(const uint8_t*, size_t, size_t, size_t)>*)funcPtr;
        func(image, width, height, pitch);
    };

    REQUIRE(callbacks_.fetchWindowImage != nullptr);
    callbacks_.fetchWindowImage(callbackData_, window, callFunc, (void*)&func);
}

void Context::onWindowManagerResizeWindow(
    uint64_t window,
    size_t width,
    size_t height
) {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);
    REQUIRE(window);

    width = max(width, (size_t)1);
    height = max(height, (size_t)1);

    REQUIRE(callbacks_.resizeWindow != nullptr);
    callbacks_.resizeWindow(callbackData_, window, width, height);
}

#define FORWARD_WINDOW_EVENT(src, callback, callbackArgs) \
    void Context::src { \
        REQUIRE(threadRunningPumpEvents); \
        REQUIRE(state_ == Running); \
        REQUIRE(window); \
         \
        REQUIRE(callbacks_.callback != nullptr); \
        callbacks_.callback callbackArgs; \
    }

FORWARD_WINDOW_EVENT(
    onWindowManagerMouseDown(uint64_t window, int x, int y, int button),
    mouseDown, (callbackData_, window, x, y, button)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerMouseUp(uint64_t window, int x, int y, int button),
    mouseUp, (callbackData_, window, x, y, button)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerMouseMove(uint64_t window, int x, int y),
    mouseMove, (callbackData_, window, x, y)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerMouseDoubleClick(uint64_t window, int x, int y, int button),
    mouseDoubleClick, (callbackData_, window, x, y, button)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerMouseWheel(uint64_t window, int x, int y, int delta),
    mouseWheel, (callbackData_, window, x, y, 0, -delta)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerMouseLeave(uint64_t window, int x, int y),
    mouseLeave, (callbackData_, window, x, y)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerKeyDown(uint64_t window, int key),
    keyDown, (callbackData_, window, key)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerKeyUp(uint64_t window, int key),
    keyUp, (callbackData_, window, key)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerLoseFocus(uint64_t window),
    loseFocus, (callbackData_, window)
)
FORWARD_WINDOW_EVENT(
    onWindowManagerNavigate(uint64_t window, int direction),
    navigate, (callbackData_, window, direction)
)

void Context::onWindowManagerNavigateToURI(uint64_t window, string uri) {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);
    REQUIRE(window);

    if(!uriNavigationCallbacks_) {
        WARNING_LOG(
            "Window navigation to URI denied because the program has not enabled URINavigation "
            "vice plugin API extension"
        );
        return;
    }

    REQUIRE(uriNavigationCallbacks_->navigateWindowToURI != nullptr);
    uriNavigationCallbacks_->navigateWindowToURI(callbackData_, window, uri.c_str());
}

void Context::onWindowManagerUploadFile(
    uint64_t window, string name, shared_ptr<FileUpload> file
) {
    REQUIRE(threadRunningPumpEvents);
    REQUIRE(state_ == Running);
    REQUIRE(window);

    PathStr path = file->path();
    string pathUTF8 = pathToUTF8(path);

    REQUIRE(callbacks_.uploadFile != nullptr);
    callbacks_.uploadFile(
        callbackData_,
        window,
        name.c_str(),
        pathUTF8.c_str(),
        [](void* cleanupData) {
            shared_ptr<FileUpload>* file = (shared_ptr<FileUpload>*)cleanupData;
            delete file;
        },
        (void*)new shared_ptr<FileUpload>(file)
    );
}

FORWARD_WINDOW_EVENT(
    onWindowManagerCancelFileUpload(uint64_t window),
    cancelFileUpload, (callbackData_, window)
)

void Context::handleClipboardHTTPRequest_(MCE,
    shared_ptr<HTTPRequest> request
) {
    REQUIRE(state_ == Running);

    auto sendPage = [&](const string& text) {
        request->sendHTMLResponse(
            200,
            writeClipboardHTML,
            {programName_, htmlEscapeString(text), clipboardCSRFToken_}
        );
    };

    string method = request->method();
    if(method == "GET") {
        sendPage("");
    } else if(method == "POST") {
        REQUIRE(!clipboardCSRFToken_.empty());
        if(request->getFormParam("csrftoken") != clipboardCSRFToken_) {
            request->sendTextResponse(403, "ERROR: Invalid CSRF token\n");
            return;
        }

        string mode = request->getFormParam("mode");
        if(mode == "get") {
            REQUIRE(callbacks_.requestClipboardContent != nullptr);
            int result = callbacks_.requestClipboardContent(callbackData_);
            REQUIRE(result == 0 || result == 1);

            if(result) {
                clipboardRequests_.push_back(request);
                startClipboardTimeout_();
            } else {
                sendPage("");
            }
        } else if(mode == "set") {
            string text = request->getFormParam("text");
            text = sanitizeUTF8String(text);

            REQUIRE(callbacks_.copyToClipboard != nullptr);
            callbacks_.copyToClipboard(callbackData_, text.c_str());

            sendPage(text);
        } else {
            request->sendTextResponse(400, "ERROR: Invalid request parameters");
        }
    } else {
        request->sendTextResponse(400, "ERROR: Invalid request method");
    }
}

void Context::startClipboardTimeout_() {
    REQUIRE(state_ == Running);

    if(clipboardTimeout_) {
        return;
    }

    shared_ptr<Context> self = shared_from_this();
    clipboardTimeout_ = postDelayedTask(
        milliseconds(1000),
        [self]() {
            REQUIRE(self->clipboardTimeout_);
            self->clipboardTimeout_.reset();

            vector<shared_ptr<HTTPRequest>> requests;
            swap(self->clipboardRequests_, requests);
            for(shared_ptr<HTTPRequest> request : requests) {
                request->sendHTMLResponse(
                    200,
                    writeClipboardHTML,
                    {self->programName_, "", self->clipboardCSRFToken_}
                );
            }
        }
    );
}

}
