#include "common.hpp"

#include "include/wrapper/cef_closure_task.h"

namespace {

void callFunction(std::function<void()> func) {
    func();
};

}

void postTask(std::function<void()> func) {
    CefPostTask(TID_UI, base::Bind(callFunction, func));
}
