#define CONF_FOREACH_OPT \
    CONF_FOREACH_OPT_ITEM(vicePlugin) \
    CONF_FOREACH_OPT_ITEM(httpListenAddr) \
    CONF_FOREACH_OPT_ITEM(userAgent) \
    CONF_FOREACH_OPT_ITEM(defaultQuality) \
    CONF_FOREACH_OPT_ITEM(useDedicatedXvfb) \
    CONF_FOREACH_OPT_ITEM(startPage) \
    CONF_FOREACH_OPT_ITEM(dataDir) \
    CONF_FOREACH_OPT_ITEM(sessionLimit) \
    CONF_FOREACH_OPT_ITEM(httpAuth)

CONF_DEF_OPT_INFO(vicePlugin) {
    const char* name = "vice-plugin";
    const char* valSpec = "FILENAME";
    string desc() {
        return "vice plugin used for user interaction";
    }
    string defaultVal() {
        return "retrowebvice.so";
    }
    bool validate(const string& val) {
        return !val.empty();
    }
};

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

CONF_DEF_OPT_INFO(startPage) {
    const char* name = "start-page";
    const char* valSpec = "URL";
    string desc() {
        return "URL of the initial page for each new session";
    }
    string defaultVal() {
        return "about:blank";
    }
    bool validate(string val) {
        return !val.empty();
    }
};

CONF_DEF_OPT_INFO(dataDir) {
    const char* name = "data-dir";
    const char* valSpec = "PATH";
    string desc() {
        return "absolute path to a directory that will be used to store data such as cookies and cache; if empty, the browser runs in incognito mode";
    }
    string defaultValStr() {
        return "default empty";
    }
    string defaultVal() {
        return "";
    }
};

CONF_DEF_OPT_INFO(sessionLimit) {
    const char* name = "session-limit";
    const char* valSpec = "COUNT";
    string desc() {
        return "maximum number of sessions (browser windows) that can be open at the same time";
    }
    int defaultVal() {
        return 32;
    }
    bool validate(int val) {
        return val >= 1;
    }
};

CONF_DEF_OPT_INFO(httpAuth) {
    const char* name = "http-auth";
    const char* valSpec = "USER:PASSWORD";
    string desc() {
        return "if nonempty, the client is required to authenticate using HTTP basic authentication with given username and password; if the special value 'env' is specified, the value is read from the environment variable BROWSERVICE_HTTP_AUTH_CREDENTIALS";
    }
    string defaultValStr() {
        return "default empty";
    }
    string defaultVal() {
        return "";
    }
    optional<string> parse(string str) {
        optional<string> empty;

        if(str.empty()) {
            return string();
        } else {
            string value;
            if(str == "env") {
                const char* valuePtr = getenv("BROWSERVICE_HTTP_AUTH_CREDENTIALS");
                if(valuePtr == nullptr) {
                    ERROR_LOG("Environment variable BROWSERVICE_HTTP_AUTH_CREDENTIALS missing");
                    return empty;
                }
                value = valuePtr;
            } else {
                value = str;
            }

            size_t colonPos = value.find(':');
            if(colonPos == value.npos || !colonPos || colonPos + 1 == value.size()) {
                ERROR_LOG("Given HTTP authentication credential string is invalid");
                return empty;
            }
            return value;
        }
    }
};
