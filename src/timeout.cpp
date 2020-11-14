#include "timeout.hpp"

#include "include/wrapper/cef_closure_task.h"

Timeout::Timeout(CKey, int64_t delayMs) {
    REQUIRE_UI_THREAD();

    delayMs_ = max(delayMs, (int64_t)1);
    active_ = false;
    delayedTaskScheduled_ = false;
}

void Timeout::set(Func func) {
    REQUIRE_UI_THREAD();
    REQUIRE(!active_);

    active_ = true;
    func_ = func;
    funcTime_ = steady_clock::now() + milliseconds(delayMs_);

    if(!delayedTaskScheduled_) {
        delayedTaskScheduled_ = true;
        delayedTaskTime_ = funcTime_;

        shared_ptr<Timeout> self = shared_from_this();
        function<void()> task = [self]() {
            self->delayedTask_();
        };

        void (*call)(function<void()>) = [](function<void()> func) {
            func();
        };
        CefPostDelayedTask(TID_UI, base::Bind(call, task), delayMs_);
    }
}

void Timeout::clear(bool runFunc) {
    REQUIRE_UI_THREAD();

    if(!active_) {
        return;
    }

    active_ = false;

    Func func;
    swap(func_, func);

    if(runFunc) {
        func();
    }
}

bool Timeout::isActive() {
    REQUIRE_UI_THREAD();
    return active_;
}

void Timeout::delayedTask_() {
    REQUIRE_UI_THREAD();

    REQUIRE(delayedTaskScheduled_);
    delayedTaskScheduled_ = false;

    if(!active_) {
        return;
    }

    if(funcTime_ == delayedTaskTime_) {
        active_ = false;
        Func func;
        swap(func_, func);
        func();
    } else {
        int64_t addDelayMs = (int64_t)(duration_cast<milliseconds>(
            funcTime_ - delayedTaskTime_
        ).count());
        addDelayMs = max(addDelayMs, (int64_t)1);

        delayedTaskScheduled_ = true;
        delayedTaskTime_ = funcTime_;

        shared_ptr<Timeout> self = shared_from_this();
        function<void()> task = [self]() {
            self->delayedTask_();
        };

        void (*call)(function<void()>) = [](function<void()> func) {
            func();
        };
        CefPostDelayedTask(TID_UI, base::Bind(call, task), addDelayMs);
    }
}
