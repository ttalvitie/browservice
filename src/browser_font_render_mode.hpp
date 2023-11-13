#pragma once

#include "common.hpp"

namespace browservice {

enum class BrowserFontRenderMode {
    NoAntiAliasing,
    AntiAliasing,
    AntiAliasingSubpixelRGB,
    AntiAliasingSubpixelBGR,
    AntiAliasingSubpixelVRGB,
    AntiAliasingSubpixelVBGR,
    System
};

void initBrowserFontRenderMode(BrowserFontRenderMode mode);

vector<pair<BrowserFontRenderMode, string>> listBrowserFontRenderModes();

string getBrowserFontRenderModeDescription();

}
