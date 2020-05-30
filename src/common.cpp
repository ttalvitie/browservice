#include "common.hpp"

#include "include/wrapper/cef_closure_task.h"

void postTask(function<void()> func) {
    void (*call)(function<void()>) = [](function<void()> func) {
        func();
    };
    CefPostTask(TID_UI, base::Bind(call, func));
}

atomic<bool> requireUIThreadEnabled_(false);
