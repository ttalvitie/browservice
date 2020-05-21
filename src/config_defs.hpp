#define CONF_FOREACH_OPT \
    CONF_FOREACH_OPT_ITEM(httpListenAddr) \
    CONF_FOREACH_OPT_ITEM(userAgent) \
    CONF_FOREACH_OPT_ITEM(defaultQuality) \
    CONF_FOREACH_OPT_ITEM(useDedicatedXvfb)

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
        ss << "(" << MinQuality << ".." << (MaxQuality - 1) << " or PNG)";
        return ss.str();
    }
    int defaultVal() {
        return MaxQuality;
    }
    string defaultValStr() {
        return "default: PNG";
    }
    optional<int> parse(string str) {
        for(char& c : str) {
            c = tolower(c);
        }
        if(str == "png") {
            return MaxQuality;
        } else {
            return parseString<int>(str);
        }
    }
    bool validate(int val) {
        return val >= MinQuality && val <= MaxQuality;
    }
};

CONF_DEF_OPT_INFO(useDedicatedXvfb) {
    const char* name = "use-dedicated-xvfb";
    const char* valSpec = "YES/NO";
    string desc() {
        return
            "if enabled, the browser is run in its own Xvfb X server; "
            "otherwise, the browser shares the X session of the environment (including, e.g., the clipboard)";
    }
    bool defaultVal() {
        return true;
    }
};
