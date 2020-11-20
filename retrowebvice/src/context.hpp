#include "http.hpp"

namespace retrowebvice {

class LogForwarder {
public:
    template <typename... T>
    void operator()(const T&... args) {
        vector<string> argStrs = {toString(args)...};
        string msg;
        for(const string& argStr : argStrs) {
            msg.append(argStr);
        }
        forwarder_(msg);
        forwarder_ = [](string) {};
    }
    ~LogForwarder() {
        forwarder_("");
    }

private:
    LogForwarder(function<void(string)> forwarder)
        : forwarder_(forwarder)
    {}

    function<void(string)> forwarder_;

    friend class Context;
};

class Context {
public:
    // Returns either a successfully constructed context or an error message.
    static variant<unique_ptr<Context>, string> create(
        vector<pair<string, string>> options,
        function<void(string, string)> panicCallback,
        function<void(string, string)> infoLogCallback,
        function<void(string, string)> warningLogCallback,
        function<void(string, string)> errorLogCallback
    );

    Context(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context();

    // Panic/log "member macros" that forward to the callbacks provided by the
    // user of the plugin.
    #define PANIC panic(__FILE__ ":" STRINGIFY(__LINE__))
    #define INFO_LOG infoLog(__FILE__ ":" STRINGIFY(__LINE__))
    #define WARNING_LOG warningLog(__FILE__ ":" STRINGIFY(__LINE__))
    #define ERROR_LOG errorLog(__FILE__ ":" STRINGIFY(__LINE__))

    LogForwarder panic(string location);
    LogForwarder infoLog(string location);
    LogForwarder warningLog(string location);
    LogForwarder errorLog(string location);

    #define REQUIRE(cond) require(__FILE__ ":" STRINGIFY(__LINE__), #cond, (bool)(cond))
    inline void require(const char* location, const char* condStr, bool cond) {
        if(!cond) {
            panic(location)("Requirement '", condStr, "' failed");
        }
    }

    // Returns documentation for supported options in create as
    // (name, valSpec, desc, defaultValStr)-tuples.
    static vector<tuple<string, string, string, string>> supportedOptionDocs();

    // Functions to be wrapped in plugin API:
    void start();
    void asyncShutdown(function<void()> shutdownCompleteCallback);

    // Functions to be called by the internal subsystems:
    void handleHTTPRequest(HTTPRequest& request);

private:
    Context(
        function<void(string, string)> panicCallback,
        function<void(string, string)> infoLogCallback,
        function<void(string, string)> warningLogCallback,
        function<void(string, string)> errorLogCallback,
        SocketAddress httpListenAddr,
        int httpMaxThreads,
        string httpAuthCredentials
    );

    function<void(string, string)> panicCallback_;
    function<void(string, string)> infoLogCallback_;
    function<void(string, string)> warningLogCallback_;
    function<void(string, string)> errorLogCallback_;

    bool startedBefore_;
    bool running_;
    bool shuttingDown_;

    function<void(Context&)> initHTTPServer_;
    optional<HTTPServer> httpServer_;

    string httpAuthCredentials_;
};

}
