#pragma once

#include "common.hpp"

namespace retrojsvice {

struct NewWindowHTMLData {
    uint64_t windowHandle;
};
void writeNewWindowHTML(ostream& out, const NewWindowHTMLData& data);

}
