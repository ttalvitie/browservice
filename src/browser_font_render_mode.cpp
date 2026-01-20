#include "browser_font_render_mode.hpp"

// Accessor functions defined in patched CEF.
extern "C" void cef_chromiumBrowserviceFontRenderParamsSetAntialiasingEnabled(int enabled);
extern "C" void cef_chromiumBrowserviceFontRenderParamsSetSubpixelPositioningEnabled(int enabled);
extern "C" void cef_chromiumBrowserviceFontRenderParamsSetAutohinterEnabled(int enabled);
extern "C" void cef_chromiumBrowserviceFontRenderParamsSetUseBitmapsEnabled(int enabled);
extern "C" void cef_chromiumBrowserviceFontRenderParamsSetHinting(int val);
extern "C" void cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(int val);

namespace browservice {
namespace {

constexpr int HINTING_NONE = 0;
constexpr int HINTING_SLIGHT = 1;
constexpr int HINTING_MEDIUM = 2;
constexpr int HINTING_FULL = 3;

constexpr int SUBPIXEL_RENDERING_NONE = 0;
constexpr int SUBPIXEL_RENDERING_RGB = 1;
constexpr int SUBPIXEL_RENDERING_BGR = 2;
constexpr int SUBPIXEL_RENDERING_VRGB = 3;
constexpr int SUBPIXEL_RENDERING_VBGR = 4;

}

void initBrowserFontRenderMode(BrowserFontRenderMode mode) {
    if(mode == BrowserFontRenderMode::System) {
        return;
    }

#ifdef USE_STD_CEF
    WARNING_LOG("Custom browser font render modes are not supported with standard CEF");
#else
    cef_chromiumBrowserviceFontRenderParamsSetSubpixelPositioningEnabled(0);
    cef_chromiumBrowserviceFontRenderParamsSetAutohinterEnabled(0);
    cef_chromiumBrowserviceFontRenderParamsSetUseBitmapsEnabled(0);

    if(mode == BrowserFontRenderMode::NoAntiAliasing) {
        cef_chromiumBrowserviceFontRenderParamsSetAntialiasingEnabled(0);
        cef_chromiumBrowserviceFontRenderParamsSetHinting(HINTING_FULL);
        cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(SUBPIXEL_RENDERING_NONE);
    } else {
        cef_chromiumBrowserviceFontRenderParamsSetAntialiasingEnabled(1);
        cef_chromiumBrowserviceFontRenderParamsSetHinting(HINTING_MEDIUM);

        if(mode == BrowserFontRenderMode::AntiAliasing) {
            cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(SUBPIXEL_RENDERING_NONE);
        } else if(mode == BrowserFontRenderMode::AntiAliasingSubpixelRGB) {
            cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(SUBPIXEL_RENDERING_RGB);
        } else if(mode == BrowserFontRenderMode::AntiAliasingSubpixelBGR) {
            cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(SUBPIXEL_RENDERING_BGR);
        } else if(mode == BrowserFontRenderMode::AntiAliasingSubpixelVRGB) {
            cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(SUBPIXEL_RENDERING_VRGB);
        } else if(mode == BrowserFontRenderMode::AntiAliasingSubpixelVBGR) {
            cef_chromiumBrowserviceFontRenderParamsSetSubpixelRendering(SUBPIXEL_RENDERING_VBGR);
        } else {
            PANIC("Invalid browser font render mode");
        }
    }
#endif
}

vector<pair<BrowserFontRenderMode, string>> listBrowserFontRenderModes() {
    return {
        {BrowserFontRenderMode::NoAntiAliasing, "no-antialiasing"},
        {BrowserFontRenderMode::AntiAliasing, "antialiasing"},
        {BrowserFontRenderMode::AntiAliasingSubpixelRGB, "antialiasing-subpixel-rgb"},
        {BrowserFontRenderMode::AntiAliasingSubpixelBGR, "antialiasing-subpixel-bgr"},
        {BrowserFontRenderMode::AntiAliasingSubpixelVRGB, "antialiasing-subpixel-vrgb"},
        {BrowserFontRenderMode::AntiAliasingSubpixelVBGR, "antialiasing-subpixel-vbgr"},
        {BrowserFontRenderMode::System, "system"}
    };
}

string getBrowserFontRenderModeDescription() {
    return
        "'no-antialiasing' for unsmoothed text, "
        "'antialiasing' for smoothed text, "
        "'antialiasing-subpixel-X' where X is one of 'rgb', 'bgr', 'vrgb' and 'vbgr' for smoothed text with subpixel rendering, "
        "and 'system' for the Chromium default for this system";
}

}
