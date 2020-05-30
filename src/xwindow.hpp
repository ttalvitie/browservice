#pragma once

#include "common.hpp"

// Our X window that we can use to e.g. access clipboard
class XWindow {
SHARED_ONLY_CLASS(XWindow);
public:
    XWindow(CKey);
    ~XWindow();

    // Pasting from clipboard is a best-effort implementation; the callback may
    // not be called if the value is not available within the given timeout or
    // pasteFromClipboard or copyToClipboard is called again
    void pasteFromClipboard(function<void(string)> callback, int64_t timeoutMs);
    void copyToClipboard(string text);

private:
    class Impl;
    shared_ptr<Impl> impl_;
};
