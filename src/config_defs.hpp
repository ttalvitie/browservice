#ifdef _WIN32
#define CONF_FOREACH_OPT_ITEM_USE_DEDICATED_XVFB
#else
#define CONF_FOREACH_OPT_ITEM_USE_DEDICATED_XVFB CONF_FOREACH_OPT_ITEM(useDedicatedXvfb)
#endif

#define CONF_FOREACH_OPT \
    CONF_FOREACH_OPT_ITEM(vicePlugin) \
    CONF_FOREACH_OPT_ITEM(userAgent) \
    CONF_FOREACH_OPT_ITEM_USE_DEDICATED_XVFB \
    CONF_FOREACH_OPT_ITEM(blockFileScheme) \
    CONF_FOREACH_OPT_ITEM(startPage) \
    CONF_FOREACH_OPT_ITEM(dataDir) \
    CONF_FOREACH_OPT_ITEM(windowLimit) \
    CONF_FOREACH_OPT_ITEM(chromiumArgs) \
    CONF_FOREACH_OPT_ITEM(showSoftNavigationButtons)

CONF_DEF_OPT_INFO(vicePlugin) {
    const char* name = "vice-plugin";
    const char* valSpec = "FILENAME";
    string desc() {
        return "vice plugin to use for the user interface";
    }
    string defaultVal() {
#ifdef _WIN32
        return "retrojsvice.dll";
#else
        return "retrojsvice.so";
#endif
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

#ifndef _WIN32
CONF_DEF_OPT_INFO(useDedicatedXvfb) {
    const char* name = "use-dedicated-xvfb";
    const char* valSpec = "YES/NO";
    string desc() {
        return
            "if enabled, the browser is run in its own Xvfb X server; "
            "otherwise, the browser shares the X session of the environment";
    }
    bool defaultVal() {
        return true;
    }
};
#endif

CONF_DEF_OPT_INFO(blockFileScheme) {
    const char* name = "block-file-scheme";
    const char* valSpec = "YES/NO";
    string desc() {
        return
            "if enabled, attempts to access local files through the file:// URI scheme are blocked "
            "(WARNING: there may be ways around the block; do NOT allow untrusted users to access Browservice)";
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

CONF_DEF_OPT_INFO(chromiumArgs) {
    const char* name = "chromium-args";
    const char* valSpec = "NAME(=VAL),...";
    string desc() {
        return "comma-separated extra arguments to be forwarded to Chromium";
    }
    string defaultValStr() {
        return "default empty";
    }
    vector<pair<string, optional<string>>> defaultVal() {
        return vector<pair<string, optional<string>>>();
    }
    optional<vector<pair<string, optional<string>>>> parse(string str) {
        if(str.empty()) {
            return vector<pair<string, optional<string>>>();
        }

        optional<vector<pair<string, optional<string>>>> empty;
        vector<pair<string, optional<string>>> ret;

        size_t start = 0;
        while(true) {
            size_t end = start;
            while(end != str.size() && str[end] != ',') {
                ++end;
            }

            size_t eq = start;
            while(eq != end && str[eq] != '=') {
                ++eq;
            }

            string argName;
            optional<string> value;
            if(eq == end) {
                argName = str.substr(start, end - start);
            } else {
                argName = str.substr(start, eq - start);
                value = str.substr(eq + 1, end - eq - 1);
            }

            for(int i = 0; i < 2; ++i) {
                if(!argName.empty() && argName[0] == '-') {
                    argName = argName.substr(1);
                }
            }
            if(argName.empty()) {
                return empty;
            }

            ret.emplace_back(argName, value);

            if(end == str.size()) {
                break;
            }
            start = end + 1;
        }
        return ret;
    }
};

CONF_DEF_OPT_INFO(showSoftNavigationButtons) {
    const char* name = "show-soft-navigation-buttons";
    const char* valSpec = "YES/NO";
    string desc() {
        return
            "if enabled, navigation buttons (Back/Forward/Refresh/Home) are added to the control bar in the browser view";
    }
    bool defaultVal() {
        return false;
    }
};
