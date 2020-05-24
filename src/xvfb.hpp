#pragma once

#include "common.hpp"

#include <sys/types.h>

// Xvfb X server child process
class Xvfb {
SHARED_ONLY_CLASS(Xvfb);
public:
    Xvfb(CKey);
    ~Xvfb();

    // Setup the DISPLAY environment variable to point to this X server
    void setupEnv();

    // Shut down the X server. Run automatically at destruction.
    void shutdown();

private:
    pid_t pid_;
    int display_;
    bool running_;
};
