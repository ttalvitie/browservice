#define CONF_FOREACH_OPT \
    CONF_FOREACH_OPT_ITEM(httpListenAddr) \
    CONF_FOREACH_OPT_ITEM(userAgent)

CONF_DEF_OPT_INFO(httpListenAddr) {
    const char* name = "http-listen-addr";
    const char* desc = "bind address and port for the HTTP server";
    const char* valSpec = "IP:PORT";
    string defaultVal() {
        return "127.0.0.1:8080";
    }
    bool validate(const string& val) {
        string val2;
        try {
            val2 = Poco::Net::SocketAddress(val).toString();
        } catch(...) {
            return false;
        }
        return val == val2;
    }
};

CONF_DEF_OPT_INFO(userAgent) {
    const char* name = "user-agent";
    const char* desc = "value for the User-Agent headers sent by the embedded browser";
    const char* valSpec = "STRING";
    string defaultVal() {
        return "";
    }
    string defaultValStr() {
        return "default determined by CEF";
    }
    bool validate(const string& val) {
        return !val.empty();
    }
};
