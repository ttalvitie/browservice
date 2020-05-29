#pragma once

#include "config.hpp"

class TextRenderContext;
class XWindow;

class Globals {
SHARED_ONLY_CLASS(Globals);
public:
    Globals(CKey, shared_ptr<Config> config);

    const shared_ptr<Config> config;
    const shared_ptr<XWindow> xWindow;
    const shared_ptr<TextRenderContext> textRenderContext;
};

extern shared_ptr<Globals> globals;
