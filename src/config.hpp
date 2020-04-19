#pragma once

#include "common.hpp"

class Config {
SHARED_ONLY_CLASS(Config);
private:
    class Src;
    int dummy_;

public:
    Config(CKey, Src& src);

    // Returns empty pointer if reading the configuration failed or help was
    // shown and the program should be terminated
    static shared_ptr<Config> read(int argc, char* argv[]);

public:
    const string httpListenAddr;
    const string userAgent;
};

