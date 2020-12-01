#include "context.hpp"

#include "http.hpp"
#include "task_queue.hpp"

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

    INFO_LOG(
        "Context configured: ",
        "HTTP listen addr '", httpListenAddr, "', ",
        "HTTP max threads '", httpMaxThreads, "', ",
        "HTTP auth credentials '", httpAuthCredentials, "'"
    );

    return Context::create(CKey());
}

Context::Context(CKey, CKey) {
    INFO_LOG("Creating retrojsvice plugin context");

    state_ = Pending;
    shutdownPending_ = false;
    inAPICall_.store(false);
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
    function<void()> eventNotifyCallback,
    function<void()> shutdownCompleteCallback
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

    state_ = Running;
    shutdownCompleteCallback_ = shutdownCompleteCallback;

    taskQueue_ = TaskQueue::create(eventNotifyCallback);

    RunningAPILock runningApiLock(move(apiLock));
}

void Context::shutdown() {
    RunningAPILock apiLock(this);

    if(state_ != Running) {
        PANIC("Requesting shutdown of a plugin that is not running");
    }
    if(shutdownPending_) {
        PANIC("Requesting shutdown of a plugin that is already shutting down");
    }

    INFO_LOG("Shutting down plugin");

    shutdownPending_ = true;

    shared_ptr<Context> self = shared_from_this();
    shared_ptr<TaskQueue> taskQueue = TaskQueue::getActiveQueue();
    thread([self, taskQueue]() {
        ActiveTaskQueueLock activeTaskQueueLock(taskQueue);

        sleep_for(milliseconds(3000));

        postTask(self, &Context::shutdownComplete_);
    }).detach();
}

void Context::pumpEvents() {
    RunningAPILock apiLock(this);

    taskQueue_->runTasks();
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

void Context::shutdownComplete_() {
    REQUIRE_API_THREAD();
    REQUIRE(state_ == Running);
    REQUIRE(shutdownPending_);

    INFO_LOG("Plugin shutdown complete");

    state_ = ShutdownComplete;
    shutdownPending_ = false;

    taskQueue_->shutdown();

    shutdownCompleteCallback_();
}

}
