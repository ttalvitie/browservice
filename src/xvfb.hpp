/*
#pragma once
#include "common.hpp"

#include <sys/types.h>

namespace browservice {

class TempDir;

// Xvfb X server child process
class Xvfb {
SHARED_ONLY_CLASS(Xvfb);
public:
    Xvfb(CKey);
    ~Xvfb();

    // Setup the DISPLAY and XAUTHORITY environment variables to point to this
    // X server
    void setupEnv();

    // Shut down the X server. Run automatically at destruction.
    void shutdown();

private:
    shared_ptr<TempDir> tempDir_;
    string xAuthPath_;
    pid_t pid_;
    int display_;
    bool running_;
};

}
*/
