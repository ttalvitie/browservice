#pragma once

#include "common.hpp"

namespace browservice {

extern const char* BrowserviceVersion;

class Config {
SHARED_ONLY_CLASS(Config);
private:
    class Src;
    int dummy_;

public:
    Config(CKey, Src& src);

    // Returns empty pointer if reading the configuration failed or help/version
    // was shown and the program should be terminated
    static shared_ptr<Config> read(int argc, const char* const* argv);
    static shared_ptr<Config> read(int argc, const wchar_t* const* argv);

public:
    const vector<pair<string, string>> viceOpts;
    const string vicePlugin;
    const string userAgent;
#ifndef _WIN32
    const bool useDedicatedXvfb;
#endif
    const bool blockFileScheme;
    const string startPage;
    const string dataDir;
    const int windowLimit;
    const vector<pair<string, optional<string>>> chromiumArgs;
    bool showSoftNavigationButtons;
};

}
