#include "context.hpp"

#include "html.hpp"
#include "secrets.hpp"

namespace retrojsvice {

namespace {

const string defaultHTTPListenAddr = "127.0.0.1:8080";
const int defaultHTTPMaxThreads = 100;

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
    SocketAddress httpListenAddr =
        SocketAddress::parse(defaultHTTPListenAddr).value();
    int httpMaxThreads = defaultHTTPMaxThreads;
    string httpAuthCredentials;

    for(const pair<string, string>& option : options) {
        const string& name = option.first;
        const string& value = option.second;

        if(name == "default-quality") {
            return "Option default-quality supported but not implemented";
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
        } else {
            return "Unrecognized option '" + name + "'";
        }
    }

    return Context::create(
        CKey(),
        httpListenAddr,
        httpMaxThreads,
        httpAuthCredentials,
        programName
    );
}

Context::Context(CKey, CKey,
    SocketAddress httpListenAddr,
    int httpMaxThreads,
    string httpAuthCredentials,
    string programName
)
    : httpListenAddr_(httpListenAddr)
{
    INFO_LOG("Creating retrojsvice plugin context");

    httpMaxThreads_ = httpMaxThreads;
    httpAuthCredentials_ = httpAuthCredentials;
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
    windowManager_ =
        WindowManager::create(shared_from_this(), secretGen_, programName_);

    clipboardCSRFToken_ = secretGen_->generateCSRFToken();
}

void Context::shutdown() {
    RunningAPILock apiLock(this);

    if(shutdownPhase_ != NoPendingShutdown) {
        PANIC("Requested shutdown of a plugin that is already shutting down");
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

bool Context::createPopupWindow(
    uint64_t parentWindow,
    uint64_t popupWindow,
    char** msg
) {
    RunningAPILock apiLock(this);
    REQUIRE(!threadRunningPumpEvents);

    string reason = "Unknown reason";
    if(windowManager_->createPopupWindow(parentWindow, popupWindow, reason)) {
        return true;
    } else {
        if(msg != nullptr) {
            *msg = createMallocString(reason);
        }
        return false;
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

void Context::handleClipboardHTTPRequest_(MCE,
    shared_ptr<HTTPRequest> request
) {
    string method = request->method();
    if(method == "GET") {
        request->sendHTMLResponse(
            200,
            writeClipboardHTML,
            {programName_, "", clipboardCSRFToken_}
        );
    } else if(method == "POST") {
        REQUIRE(!clipboardCSRFToken_.empty());
        if(request->getFormParam("csrftoken") != clipboardCSRFToken_) {
            request->sendTextResponse(403, "ERROR: Invalid CSRF token\n");
            return;
        }

        string mode = request->getFormParam("mode");
        if(mode == "get") {
            request->sendTextResponse(500, "ERROR: Getting clipboard not implemented (TODO)");
        } else if(mode == "set") {
            string text = request->getFormParam("text");

            REQUIRE(callbacks_.copyToClipboard != nullptr);
            callbacks_.copyToClipboard(callbackData_, text.c_str());

            request->sendHTMLResponse(
                200,
                writeClipboardHTML,
                {programName_, htmlEscapeString(text), clipboardCSRFToken_}
            );
        } else {
            request->sendTextResponse(400, "ERROR: Invalid request parameters");
        }
    } else {
        request->sendTextResponse(400, "ERROR: Invalid request method");
    }
}

}
