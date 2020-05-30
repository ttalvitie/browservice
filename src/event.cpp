#include "event.hpp"

#include "key.hpp"
#include "widget.hpp"

namespace {

bool processParsedEvent(
    shared_ptr<Widget> widget,
    const string& name,
    int argCount,
    const int* args
) {
    auto clampCoords = [&](int& x, int& y) {
        x = max(x, -1000);
        y = max(y, -1000);

        ImageSlice viewport = widget->getViewport();
        x = min(x, viewport.width() + 1000);
        y = min(y, viewport.height() + 1000);
    };

    if(name == "MDN" && argCount == 3 && args[2] >= 0 && args[2] <= 2) {
        int x = args[0];
        int y = args[1];
        int button = args[2];
        clampCoords(x, y);
        widget->sendMouseDownEvent(x, y, button);
        return true;
    }
    if(name == "MUP" && argCount == 3 && args[2] >= 0 && args[2] <= 2) {
        int x = args[0];
        int y = args[1];
        int button = args[2];
        clampCoords(x, y);
        widget->sendMouseUpEvent(x, y, button);
        return true;
    }
    if(name == "MDBL" && argCount == 2) {
        int x = args[0];
        int y = args[1];
        clampCoords(x, y);
        widget->sendMouseDoubleClickEvent(x, y);
        return true;
    }
    if(name == "MWH" && argCount == 3) {
        int x = args[0];
        int y = args[1];
        int delta = args[2];
        clampCoords(x, y);
        delta = max(-1000, min(1000, delta));
        widget->sendMouseWheelEvent(x, y, delta);
        return true;
    }
    if(name == "MMO" && argCount == 2) {
        int x = args[0];
        int y = args[1];
        clampCoords(x, y);
        widget->sendMouseMoveEvent(x, y);
        return true;
    }
    if(name == "MOUT" && argCount == 2) {
        int x = args[0];
        int y = args[1];
        clampCoords(x, y);
        widget->sendMouseLeaveEvent(x, y);
        return true;
    }
    if(name == "KDN" && argCount == 1) {
        int key = -args[0];
        if(key < 0 && isValidKey(key)) {
            widget->sendKeyDownEvent(key);
        }
        return true;
    }
    if(name == "KUP" && argCount == 1) {
        int key = -args[0];
        if(key < 0 && isValidKey(key)) {
            widget->sendKeyUpEvent(key);
        }
        return true;
    }
    if(name == "KPR" && argCount == 1) {
        int key = args[0];
        if(key > 0 && isValidKey(key)) {
            widget->sendKeyDownEvent(key);
            widget->sendKeyUpEvent(key);
        }
        return true;
    }
    if(name == "FOUT" && argCount == 0) {
        widget->sendLoseFocusEvent();
        return true;
    }

    return false;
}

}

bool processEvent(
    shared_ptr<Widget> widget,
    string::const_iterator begin,
    string::const_iterator end
) {
    requireUIThread();
    CHECK(begin < end && *(end - 1) == '/');

    string name;

    const int MaxArgCount = 3;
    int args[MaxArgCount];
    int argCount = 0;

    string::const_iterator pos = begin;
    while(*pos != '/' && *pos != '_') {
        ++pos;
    }
    name = string(begin, pos);

    bool ok = true;
    if(*pos == '_') {
        ++pos;
        while(true) {
            if(argCount == MaxArgCount) {
                ok = false;
                break;
            }
            string::const_iterator argStart = pos;
            while(*pos != '/' && *pos != '_') {
                ++pos;
            }
            optional<int> arg = parseString<int>(argStart, pos);
            if(!arg) {
                ok = false;
                break;
            }
            args[argCount++] = *arg;
            if(*pos == '/') {
                break;
            }
            ++pos;
        }
    }

    return ok && processParsedEvent(widget, name, argCount, args);
}
