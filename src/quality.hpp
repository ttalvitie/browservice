#pragma once

// Integer values in range [MinQuality, MaxQuality] are valid image
// qualities. If quality is equal to MaxQuality, then PNG is used;
// otherwise, the quality is a JPEG image quality value.
constexpr int MinQuality = 10;
constexpr int MaxQuality = 101;
