#include "context.hpp"

namespace retrojsvice {

variant<shared_ptr<Context>, string> Context::init(
    vector<pair<string, string>> options
) {
    for(const pair<string, string>& option : options) {
        const string& name = option.first;
        const string& value = option.second;

        if(
            name != "default-quality" &&
            name != "http-listen-addr" &&
            name != "http-max-threads" &&
            name != "http-auth"
        ) {
            return "Unknown option '" + name + "'";
        }
        if(value == "") {
            return "Invalid value '" + value + "' for option '" + name + "'";
        }
    }
    return Context::create(CKey());
}

Context::Context(CKey, CKey) {
    INFO_LOG("Creating retrojsvice plugin context");

    state_ = Pending;
    shutdownPending_ = false;
}

Context::~Context() {
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
    if(state_ == Running) {
        PANIC("Starting a plugin context that is already running");
    }
    if(state_ == ShutdownComplete) {
        PANIC("Starting a plugin that has already been shut down");
    }
    REQUIRE(state_ == Pending);

    INFO_LOG("Starting plugin");

    state_ = Running;
    eventNotifyCallback_ = eventNotifyCallback;
    shutdownCompleteCallback_ = shutdownCompleteCallback;
}

void Context::shutdown() {
    if(state_ != Running) {
        PANIC("Requesting shutdown of a plugin that is not running");
    }
    if(shutdownPending_) {
        PANIC("Requesting shutdown of a plugin that is already shutting down");
    }

    INFO_LOG("Shutting down plugin");

    shutdownPending_ = true;

    thread([this]() {
        sleep_for(milliseconds(3000));
        eventNotifyCallback_();
    }).detach();
}

void Context::pumpEvents() {
    if(shutdownPending_) {
        REQUIRE(state_ == Running);

        INFO_LOG("Plugin shutdown complete");

        state_ = ShutdownComplete;
        shutdownPending_ = false;
        shutdownCompleteCallback_();
    }
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
        "default: 127.0.0.1:8080"
    );
    ret.emplace_back(
        "http-max-threads",
        "COUNT",
        "maximum number of HTTP server threads",
        "default: 100"
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

}
