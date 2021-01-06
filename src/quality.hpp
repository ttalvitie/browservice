#pragma once

#include "common.hpp"

namespace browservice {

// Integer values in range [MinQuality, MaxQuality] are valid image
// qualities. If quality is equal to MaxQuality, then PNG is used;
// otherwise, the quality is a JPEG image quality value.
constexpr int MinQuality = 10;
constexpr int MaxQuality = 101;

bool hasPNGSupport(string userAgent);

// Adjusted values for globals->config->defaultQuality and MaxQuality taking
// into account the possibility of disabling PNG
int getDefaultQuality(bool allowPNG);
int getMaxQuality(bool allowPNG);

}
