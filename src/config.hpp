#pragma once

#include "common.hpp"

extern const char* BrowserviceVersion;

class Config {
SHARED_ONLY_CLASS(Config);
private:
    class Src;

public:
    Config(CKey, Src& src);

    // Returns empty pointer if reading the configuration failed or help/version
    // was shown and the program should be terminated
    static shared_ptr<Config> read(int argc, char* argv[]);

public:
    const vector<pair<string, string>> viceOpts;
    const string vicePlugin;
    const string httpListenAddr;
    const string userAgent;
    const int defaultQuality;
    const bool useDedicatedXvfb;
    const string startPage;
    const string dataDir;
    const int sessionLimit;
    const string httpAuth;
};

