#pragma once

#ifdef _WIN32

#include "common.hpp"

namespace browservice {

void copyToClipboard(string str);
string pasteFromClipboard();

}

#endif
