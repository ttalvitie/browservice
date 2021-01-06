#pragma once

#include "common.hpp"

namespace browservice {

// Timeout that runs a given callback from the CEF UI thread event loop after a
// specified (fixed) delay, unless canceled
class Timeout : public enable_shared_from_this<Timeout> {
SHARED_ONLY_CLASS(Timeout);
public:
    typedef function<void()> Func;

    Timeout(CKey, int64_t delayMs);

    // Set func to be run in delayMs milliseconds or when cleared with
    // runFunc set to true. Calling when the timeout is active is an error.
    void set(Func func);

    // If the timeout is active, stop it. If runFunc is true, the associated
    // function is called immediately.
    void clear(bool runFunc);

    // Returns true if the timeout is active, i.e. a function has been scheduled
    // to run (and not cleared).
    bool isActive();

private:
    void delayedTask_();

    int64_t delayMs_;

    bool active_;
    Func func_;
    steady_clock::time_point funcTime_;

    bool delayedTaskScheduled_;
    steady_clock::time_point delayedTaskTime_;
};

}
