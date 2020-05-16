#define CONF_FOREACH_OPT \
    CONF_FOREACH_OPT_ITEM(httpListenAddr) \
    CONF_FOREACH_OPT_ITEM(userAgent) \
    CONF_FOREACH_OPT_ITEM(defaultQuality)

CONF_DEF_OPT_INFO(httpListenAddr) {
    const char* name = "http-listen-addr";
    const char* valSpec = "IP:PORT";
    string desc() {
        return "bind address and port for the HTTP server";
    }
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
    const char* valSpec = "STRING";
    string desc() {
        return "value for the User-Agent headers sent by the embedded browser";
    }
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

CONF_DEF_OPT_INFO(defaultQuality) {
    const char* name = "default-quality";
    const char* valSpec = "QUALITY";
    string desc() {
        stringstream ss;
        ss << "initial image quality for each session ";
        ss << "(" << MinQuality << ".." << (MaxQuality - 1) << " or 'PNG')";
        return ss.str();
    }
    int defaultVal() {
        return MaxQuality;
    }
    string defaultValStr() {
        return "default PNG";
    }
    optional<int> parse(const string& str) {
        if(str == "PNG") {
            return MaxQuality;
        } else {
            return parseString<int>(str);
        }
    }
    bool validate(int val) {
        return val >= MinQuality && val <= MaxQuality;
    }
};
