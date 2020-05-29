#include "globals.hpp"

#include "quality.hpp"
#include "text.hpp"
#include "xwindow.hpp"

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
      xWindow(XWindow::create()),
      textRenderContext(TextRenderContext::create())
{
    CHECK(config);
}

shared_ptr<Globals> globals;
