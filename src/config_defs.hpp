#define CONF_FOREACH_OPT \
    CONF_FOREACH_OPT_ITEM(vicePlugin) \
    CONF_FOREACH_OPT_ITEM(userAgent) \
    CONF_FOREACH_OPT_ITEM(useDedicatedXvfb) \
    CONF_FOREACH_OPT_ITEM(startPage) \
    CONF_FOREACH_OPT_ITEM(dataDir) \
    CONF_FOREACH_OPT_ITEM(windowLimit)

CONF_DEF_OPT_INFO(vicePlugin) {
    const char* name = "vice-plugin";
    const char* valSpec = "FILENAME";
    string desc() {
        return "vice plugin to use for the user interface";
    }
    string defaultVal() {
        return "retrojsvice.so";
    }
    bool validate(const string& val) {
        return !val.empty();
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
        return "URL of the initial page for each new window";
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

CONF_DEF_OPT_INFO(windowLimit) {
    const char* name = "window-limit";
    const char* valSpec = "COUNT";
    string desc() {
        return "maximum number of browser windows that can be open at the same time";
    }
    int defaultVal() {
        return 32;
    }
    bool validate(int val) {
        return val >= 1;
    }
};
