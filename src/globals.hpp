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
#ifdef _WIN32
    const wstring dotDirPath;
#else
    const shared_ptr<XWindow> xWindow;
    const string dotDirPath;
#endif
    const shared_ptr<TextRenderContext> textRenderContext;
};

extern shared_ptr<Globals> globals;

}
