#pragma once

#include "config.hpp"

class TextRenderContext;

class Globals {
SHARED_ONLY_CLASS(Globals);
public:
    Globals(CKey, shared_ptr<Config> config);

    const shared_ptr<Config> config;
    const shared_ptr<TextRenderContext> textRenderContext;

    const int defaultQuality;
};

extern shared_ptr<Globals> globals;
