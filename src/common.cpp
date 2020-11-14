#include "common.hpp"

#include "include/wrapper/cef_closure_task.h"

static atomic<bool> panicUsingCEFFatalError_(false);

void Panicker::panic_(string msg) {
    stringstream output;
    output << "PANIC @ " << location_;
    if(!msg.empty()) {
        output << ": " << msg;
    }
    output << "\n";

    cerr << output.str();
    cerr.flush();

    if(panicUsingCEFFatalError_.load()) {
        LOG(FATAL);
    }

    abort();
}

void enablePanicUsingCEFFatalError() {
    panicUsingCEFFatalError_.store(true);
}

void postTask(function<void()> func) {
    void (*call)(function<void()>) = [](function<void()> func) {
        func();
    };
    CefPostTask(TID_UI, base::Bind(call, func));
}

atomic<bool> requireUIThreadEnabled_(false);
