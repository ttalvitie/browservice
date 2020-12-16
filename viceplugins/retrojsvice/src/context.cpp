#include "context.hpp"

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
    vector<pair<string, string>> options
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
        httpAuthCredentials
    );
}

Context::Context(CKey, CKey,
    SocketAddress httpListenAddr,
    int httpMaxThreads,
    string httpAuthCredentials
)
    : httpListenAddr_(httpListenAddr)
{
    INFO_LOG("Creating retrojsvice plugin context");

    httpMaxThreads_ = httpMaxThreads;
    httpAuthCredentials_ = httpAuthCredentials;

    state_ = Pending;
    shutdownPhase_ = NoPendingShutdown;
    inAPICall_.store(false);

    callbackData_ = nullptr;

    // Default callbacks:

    // These callbacks are always set in start
    eventNotifyCallback_ = [](void*) { PANIC(); };
    shutdownCompleteCallback_ = [](void*) { PANIC(); };

    // By default, allow no windows to be created
    createWindowCallback_ = [](void*, char** msg) -> uint64_t {
        if(msg != nullptr) {
            *msg = createMallocString("Backend callback not available");
        }
        return 0;
    };
    closeWindowCallback_ = [](void*, uint64_t) { PANIC(); };
    resizeWindowCallback_ = [](void*, uint64_t, int, int) { PANIC(); };
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
    void (*eventNotifyCallback)(void*),
    void (*shutdownCompleteCallback)(void*),
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

    REQUIRE(eventNotifyCallback != nullptr);
    REQUIRE(shutdownCompleteCallback != nullptr);

    INFO_LOG("Starting plugin");

    callbackData_ = callbackData;

    eventNotifyCallback_ = eventNotifyCallback;
    shutdownCompleteCallback_ = shutdownCompleteCallback;

    state_ = Running;
    taskQueue_ = TaskQueue::create(shared_from_this());

    RunningAPILock runningApiLock(move(apiLock));

    httpServer_ = HTTPServer::create(
        shared_from_this(),
        httpListenAddr_,
        httpMaxThreads_
    );
}

void Context::shutdown() {
    RunningAPILock apiLock(this);

    if(shutdownPhase_ != NoPendingShutdown) {
        PANIC("Requested shutdown of a plugin that is already shutting down");
    }

    INFO_LOG("Shutting down plugin");

    shutdownPhase_ = WaitHTTPServer;

    REQUIRE(httpServer_);
    httpServer_->shutdown();
}

void Context::pumpEvents() {
    RunningAPILock apiLock(this);

    taskQueue_->runTasks();
}

#define SET_CALLBACK_CHECKS() \
    APILock apiLock(this); \
    do { \
        if(state_ != Pending) { \
            PANIC( \
                "Program is setting callbacks for a plugin context that has " \
                "already been started" \
            ); \
        } \
    } while(false)

void Context::setWindowCallbacks(
    uint64_t (*createWindowCallback)(void*, char** msg),
    void (*closeWindowCallback)(void*, uint64_t handle),
    void (*resizeWindowCallback)(void*, uint64_t handle, int width, int height)
) {
    SET_CALLBACK_CHECKS();

    REQUIRE(createWindowCallback != nullptr);
    REQUIRE(closeWindowCallback != nullptr);
    REQUIRE(resizeWindowCallback != nullptr);

    createWindowCallback_ = createWindowCallback;
    closeWindowCallback_ = closeWindowCallback;
    resizeWindowCallback_ = resizeWindowCallback;
}

void Context::closeWindow(uint64_t handle) {
    RunningAPILock apiLock(this);

    PANIC("Not implemented");
}

vector<tuple<string, string, string, string>> Context::getOptionDocs() {
    vector<tuple<string, string, string, string>> ret;

    ret.emplace_back(
        "default-quality",
        "QUALITY",
        "initial image quality for each session (10..100 or PNG)",
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

static bool passwordsEqual(const void* a, const void* b, size_t size) {
    const unsigned char* x = (const unsigned char*)a;
    const unsigned char* y = (const unsigned char*)b;
    volatile unsigned char agg = 0;
    for(volatile size_t i = 0; i < size; ++i) {
        agg |= x[i] ^ y[i];
    }
    return !agg;
}

void Context::onHTTPServerRequest(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();
    REQUIRE(state_ == Running);

    if(!httpAuthCredentials_.empty()) {
        optional<string> reqCred = request->getBasicAuthCredentials();
        if(
            !reqCred ||
            reqCred->size() != httpAuthCredentials_.size() ||
            !passwordsEqual(
                (const void*)reqCred->data(),
                (const void*)httpAuthCredentials_.data(),
                httpAuthCredentials_.size()
            )
        ) {
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

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        handleNewWindowRequest_(request);
        return;
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method\n");
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
    eventNotifyCallback_(callbackData_);
}

void Context::onTaskQueueShutdownComplete() {
    REQUIRE_API_THREAD();
    REQUIRE(state_ == Running);
    REQUIRE(shutdownPhase_ == WaitTaskQueue);

    state_ = ShutdownComplete;
    shutdownPhase_ = NoPendingShutdown;

    INFO_LOG("Plugin shutdown complete");

    shutdownCompleteCallback_(callbackData_);
}

void Context::handleNewWindowRequest_(shared_ptr<HTTPRequest> request) {
    char* msgC = nullptr;
    uint64_t handle = createWindowCallback_(callbackData_, &msgC);

    if(handle) {
        INFO_LOG("Creating window ", handle);

        REQUIRE(msgC == nullptr);
        if(windows_.count(handle)) {
            PANIC("Program supplied a window handle that is already in use");
        }

        shared_ptr<Window> window = Window::create(handle);
        REQUIRE(windows_.emplace(handle, window).second);
    } else {
        REQUIRE(msgC != nullptr);
        string msg = msgC;
        free(msgC);

        INFO_LOG("Window creation denied by program (reason: ", msg, ")");

        request->sendTextResponse(
            503, "ERROR: Could not create window, reason: " + msg + "\n"
        );
    }
}

}
