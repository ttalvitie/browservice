#pragma once

#include "common.hpp"

namespace retrojsvice {

void renderUploadModeGUI(
    vector<uint8_t>& data,
    size_t width,
    size_t height,
    bool cancelButtonDown
);

bool isOverUploadModeCancelButton(
    size_t x, size_t y, size_t width, size_t height
);

}
