#pragma once

#include "config.hpp"

class TextRenderContext;

class Globals {
SHARED_ONLY_CLASS(Globals);
public:
    Globals(CKey, shared_ptr<Config> config);

    const shared_ptr<Config> config;
    const shared_ptr<TextRenderContext> textRenderContext;
};

extern shared_ptr<Globals> globals;
