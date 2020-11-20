#include "context.hpp"

namespace retrowebvice {

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

bool passwordsEqual(const void* a, const void* b, size_t size) {
    const unsigned char* x = (const unsigned char*)a;
    const unsigned char* y = (const unsigned char*)b;
    volatile unsigned char agg = 0;
    for(volatile size_t i = 0; i < size; ++i) {
        agg |= x[i] ^ y[i];
    }
    return !agg;
}

}

variant<unique_ptr<Context>, string> Context::create(
    vector<pair<string, string>> options,
    function<void(string, string)> panicCallback,
    function<void(string, string)> infoLogCallback,
    function<void(string, string)> warningLogCallback,
    function<void(string, string)> errorLogCallback
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

    return unique_ptr<Context>(
        new Context(
            panicCallback,
            infoLogCallback,
            warningLogCallback,
            errorLogCallback,
            httpListenAddr,
            httpMaxThreads,
            move(httpAuthCredentials)
        )
    );
}

Context::Context(
    function<void(string, string)> panicCallback,
    function<void(string, string)> infoLogCallback,
    function<void(string, string)> warningLogCallback,
    function<void(string, string)> errorLogCallback,
    SocketAddress httpListenAddr,
    int httpMaxThreads,
    string httpAuthCredentials
)
    : panicCallback_(panicCallback),
      infoLogCallback_(infoLogCallback),
      warningLogCallback_(warningLogCallback),
      errorLogCallback_(errorLogCallback),
      startedBefore_(false),
      running_(false),
      shuttingDown_(false),
      initHTTPServer_([httpListenAddr, httpMaxThreads](Context& self) {
          self.REQUIRE(!self.httpServer_);
          self.httpServer_.emplace(self, httpListenAddr, httpMaxThreads);
      }),
      httpAuthCredentials_(move(httpAuthCredentials))
{}

Context::~Context() {
    if(shuttingDown_) {
        PANIC("Destroying a plugin context while its shutdown is still pending");
    }
    if(running_) {
        PANIC("Destroying a running plugin context before shutting it down");
    }
}

LogForwarder Context::panic(string location) {
    function<void(string, string)> panicCallback = panicCallback_;
    return LogForwarder([location, panicCallback](string msg) {
        panicCallback(location, msg);
    });
}

LogForwarder Context::infoLog(string location) {
    function<void(string, string)> infoLogCallback = infoLogCallback_;
    return LogForwarder([location, infoLogCallback](string msg) {
        infoLogCallback(location, msg);
    });
}

LogForwarder Context::warningLog(string location) {
    function<void(string, string)> warningLogCallback = warningLogCallback_;
    return LogForwarder([location, warningLogCallback](string msg) {
        warningLogCallback(location, msg);
    });
}

LogForwarder Context::errorLog(string location) {
    function<void(string, string)> errorLogCallback = errorLogCallback_;
    return LogForwarder([location, errorLogCallback](string msg) {
        errorLogCallback(location, msg);
    });
}

vector<tuple<string, string, string, string>> Context::supportedOptionDocs() {
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

void Context::start() {
    if(startedBefore_) {
        PANIC("Requested starting a plugin context that has already been started before");
    }

    startedBefore_ = true;
    running_ = true;

    initHTTPServer_(*this);
}

void Context::asyncShutdown(function<void()> shutdownCompleteCallback) {
    if(shuttingDown_) {
        PANIC("Requested shutting down a plugin context that is already shutting down");
    }
    if(!running_) {
        PANIC("Requested shutting down a plugin context that is not running");
    }

    shuttingDown_ = true;

    REQUIRE(httpServer_);
    httpServer_->asyncShutdown([this, shutdownCompleteCallback]() {
        // NOTE: Not synchronized.
        // However, no races because HTTP requests and API calls have stopped
        // at this point.
        REQUIRE(running_);
        REQUIRE(shuttingDown_);
        running_ = false;
        shuttingDown_ = false;

        shutdownCompleteCallback();
    });
}

void Context::handleHTTPRequest(HTTPRequest& request) {
    // NOTE: Not synchronized (called from HTTP server worker thread).

    // No race, as running_ is only written to when shutdown is complete, which
    // is not possible while this function is executing.
    REQUIRE(running_);

    if(!httpAuthCredentials_.empty()) {
        optional<string> credentials = request.getBasicAuthCredentials();
        if(
            !credentials ||
            credentials->size() != httpAuthCredentials_.size() ||
            !passwordsEqual(
                (const void*)credentials->data(),
                (const void*)httpAuthCredentials_.data(),
                httpAuthCredentials_.size()
            )
        ) {
            request.sendTextResponse(
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

    request.sendTextResponse(
        200,
        string("retrowebvice.so HTTP server is working!\n") +
        "Method: " + request.method() + "\n" +
        "Path: " + request.path() + "\n" +
        "User agent: " + request.userAgent() + "\n" +
        "Form param 'x': " + request.getFormParam("x") + "\n" +
        "Form param 'y': " + request.getFormParam("y") + "\n"
    );
}

}
