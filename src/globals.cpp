#include "globals.hpp"

#include "quality.hpp"
#include "text.hpp"

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
      textRenderContext(TextRenderContext::create()),
      defaultQuality(MaxQuality)
{
    CHECK(config);
}

shared_ptr<Globals> globals;
