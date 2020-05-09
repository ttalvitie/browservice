#pragma once

#include "config.hpp"

class TextRenderContext;

class Globals {
SHARED_ONLY_CLASS(Globals);
public:
    Globals(CKey, shared_ptr<Config> config);

    const shared_ptr<Config> config;
    const shared_ptr<TextRenderContext> textRenderContext;

    // Integer values in range [minQuality, maxQuality] are valid image
    // qualities. If quality is equal to PngQuality, then PNG is used;
    // otherwise, the quality is a JPEG image quality value.
    const int minQuality;
    const int maxQuality;
    const int defaultQuality;
    static const int PNGQuality;
};

extern shared_ptr<Globals> globals;
