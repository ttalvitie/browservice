#include "globals.hpp"

#include "text.hpp"

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
      textRenderContext(TextRenderContext::create()),
      minQuality(10),
      maxQuality(101),
      defaultQuality(101)
{
    CHECK(config);
}

const int Globals::PNGQuality = 101;

shared_ptr<Globals> globals;
