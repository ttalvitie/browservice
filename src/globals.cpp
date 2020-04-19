#include "globals.hpp"

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config)
{
    CHECK(config);
}

shared_ptr<Globals> globals;
