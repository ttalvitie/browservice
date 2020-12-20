#pragma once

#include "common.hpp"

namespace retrojsvice {

// Integer values in range [MinQuality, MaxQuality] are valid image
// qualities. If quality is equal to MaxQuality, then PNG is used;
// otherwise, the quality is a JPEG image quality value.
constexpr int MinQuality = 10;
constexpr int MaxQuality = 101;

bool hasPNGSupport(string userAgent);

// Adjusted value for MaxQuality taking into account the possibility of
// disabling PNG
int getMaxQuality(bool allowPNG);

}
