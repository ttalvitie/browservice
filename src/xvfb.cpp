#include "xvfb.hpp"

#include <csignal>

#include <unistd.h>
#include <sys/wait.h>

namespace {

optional<int> parseDisplay(string displayStr) {
    optional<int> empty;
    if(displayStr.empty() || displayStr[displayStr.size() - 1] != '\n') {
        return empty;
    }
    optional<int> display = parseString<int>(
        displayStr.substr(0, displayStr.size() - 1)
    );
    if(display && *display >= 0) {
        return display;
    } else {
        return empty;
    }
}

}

Xvfb::Xvfb(CKey) {
    LOG(INFO) << "Starting Xvfb X server as child process";

    // Pipe through which Xvfb sends us the display number
    int displayFds[2];
    CHECK(!pipe(displayFds));
    int readDisplayFd = displayFds[0];
    int writeDisplayFd = displayFds[1];

    pid_ = fork();
    CHECK(pid_ != -1);
    if(!pid_) {
        // Xvfb subprocess:
        CHECK(!close(readDisplayFd));

        // Move the X server process to its own process group, as otherwise
        // Ctrl+C sent to the parent would stop the X server before we have
        // time to shut the parent down
        CHECK(!setpgid(0, 0));

        string writeDisplayFdStr = toString(writeDisplayFd);
        execlp("Xvfb", "Xvfb", "-displayfd", writeDisplayFdStr.c_str(), (char*)nullptr);

        // If exec succeeded, this should not be reachable
        CHECK(false);
    }

    // Parent process:
    CHECK(!close(writeDisplayFd));

    string displayStr;
    const size_t BufSize = 64;
    char buf[BufSize];

    while(true) {
        ssize_t readCount = read(readDisplayFd, buf, BufSize);
        if(readCount == 0) {
            break;
        }
        if(readCount < 0) {
            CHECK(errno == EINTR);
            readCount = 0;
        }
        displayStr.append(buf, readCount);
    }

    optional<int> display = parseDisplay(displayStr);
    if(!display) {
        LOG(ERROR) << "Starting Xvfb failed";
        CHECK(false);
    }

    display_ = *display;
    LOG(INFO) << "Xvfb X server :" << display_ << " successfully started";

    running_ = true;
}

Xvfb::~Xvfb() {
    if(running_) {
        shutdown();
    }
}

void Xvfb::setupEnv() {
    string displayStr = ":" + toString(display_);
    CHECK(!setenv("DISPLAY", displayStr.c_str(), true));
}

void Xvfb::shutdown() {
    if(!running_) {
        return;
    }

    LOG(INFO) << "Sending SIGTERM to the Xvfb X server child process to shut it down";
    if(kill(pid_, SIGTERM) != 0) {
        LOG(WARNING) << "Could not send SIGTERM signal to Xvfb, maybe it has already shut down?";
    }

    LOG(INFO) << "Waiting for Xvfb child process to shut down";
    CHECK(waitpid(pid_, nullptr, 0) == pid_);

    LOG(INFO) << "Successfully shut down Xvfb X server";

    running_ = false;
}
