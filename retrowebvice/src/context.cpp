#include "context.hpp"

namespace retrowebvice {

const string defaultHttpListenAddr = "127.0.0.1:8080";

variant<unique_ptr<Context>, string> Context::create(
    vector<pair<string, string>> options,
    function<void(string, string)> panicCallback,
    function<void(string, string)> infoLogCallback,
    function<void(string, string)> warningLogCallback,
    function<void(string, string)> errorLogCallback
) {
    Poco::Net::SocketAddress httpListenAddr(defaultHttpListenAddr);

    for(const pair<string, string>& option : options) {
        const string& name = option.first;
        const string& value = option.second;

        if(name == "default-quality") {
            return "Option default-quality supported but not implemented";
        } else if(name == "http-auth") {
            return "Option http-auth supported but not implemented";
        } else if(name == "http-listen-addr") {
            try {
                httpListenAddr = Poco::Net::SocketAddress(value);
                httpListenAddr.toString();
            } catch(...) {
                return "Invalid value '" + value + "' for option http-listen-addr";
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
            httpListenAddr
        )
    );
}

Context::Context(
    function<void(string, string)> panicCallback,
    function<void(string, string)> infoLogCallback,
    function<void(string, string)> warningLogCallback,
    function<void(string, string)> errorLogCallback,
    Poco::Net::SocketAddress httpListenAddr
)
    : panicCallback_(panicCallback),
      infoLogCallback_(infoLogCallback),
      warningLogCallback_(warningLogCallback),
      errorLogCallback_(errorLogCallback),
      httpListenAddr_(httpListenAddr)
{
    INFO_LOG("Creating retrowebvice vice plugin context, listening to '", httpListenAddr_, "'");
}

Context::~Context() {
    INFO_LOG("Destroying retrowebvice vice plugin context");
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
        "default: " + defaultHttpListenAddr
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

}
