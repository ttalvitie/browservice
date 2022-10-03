#pragma once

#include "config.hpp"

namespace browservice {

class TextRenderContext;
class XWindow;

class Globals {
SHARED_ONLY_CLASS(Globals);
public:
    Globals(CKey, shared_ptr<Config> config);

    const shared_ptr<Config> config;
    const PathStr dotDirPath;
    const shared_ptr<TextRenderContext> textRenderContext;
};

extern shared_ptr<Globals> globals;

}
