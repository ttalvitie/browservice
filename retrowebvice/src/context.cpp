#include "context.hpp"

namespace retrowebvice {

const string defaultHTTPListenAddr = "127.0.0.1:8080";
const int defaultHTTPMaxThreads = 100;

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
            return "Option http-auth supported but not implemented";
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
            httpMaxThreads
        )
    );
}

Context::Context(
    function<void(string, string)> panicCallback,
    function<void(string, string)> infoLogCallback,
    function<void(string, string)> warningLogCallback,
    function<void(string, string)> errorLogCallback,
    SocketAddress httpListenAddr,
    int httpMaxThreads
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
      })
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
        "BROWSERVICE_HTTP_AUTH_CREDENTIALS",
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
