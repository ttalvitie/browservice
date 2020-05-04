#include "globals.hpp"

#include "text.hpp"

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
      textRenderContext(TextRenderContext::create())
{
    CHECK(config);
}

shared_ptr<Globals> globals;
