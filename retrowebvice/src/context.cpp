#include "context.hpp"

namespace retrowebvice {

variant<unique_ptr<Context>, string> Context::create(
    vector<pair<string, string>> options,
    function<void(string, string)> panicCallback,
    function<void(string, string)> infoLogCallback,
    function<void(string, string)> warningLogCallback,
    function<void(string, string)> errorLogCallback
) {
    unique_ptr<Context> ctx(new Context);

    ctx->panicCallback_ = panicCallback;
    ctx->infoLogCallback_ = infoLogCallback;
    ctx->warningLogCallback_ = warningLogCallback;
    ctx->errorLogCallback_ = errorLogCallback;

    for(const pair<string, string>& option : options) {
        const string& name = option.first;
        //const string& value = option.second;

        if(name != "default-quality" && name != "http-auth" && name != "http-listen-addr") {
            return "Unrecognized option '" + name + "'";
        }
    }

    return move(ctx);
}

Context::~Context() {
    infoLogCallback_("~Context", "Destroying retrowebvice vice plugin context");
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
        "default: 127.0.0.1:8080"
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
