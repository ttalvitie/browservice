#pragma once

#include "common.hpp"

// Our X Window that we can use to e.g. access clipboard
class XWindow {
SHARED_ONLY_CLASS(XWindow);
public:
    XWindow(CKey);
    ~XWindow();

    // Get the X window handle (cast to uint64_t to avoid needing X11 headers
    // in this header)
    uint64_t handle();

private:
    class Impl;
    unique_ptr<Impl> impl_;
};
