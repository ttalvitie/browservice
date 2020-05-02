#pragma once

#include "common.hpp"

class Widget;

// Parse event string given by range [begin, end). If successful, return true
// and send the event to widget, otherwise return false.
bool processEvent(
    shared_ptr<Widget> widget,
    string::const_iterator begin,
    string::const_iterator end
);
