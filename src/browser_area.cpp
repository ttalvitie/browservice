#include "browser_area.hpp"

BrowserArea::BrowserArea(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler)
    : Widget(widgetEventHandler)
{
    CEF_REQUIRE_UI_THREAD();
}

void BrowserArea::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    viewport.fill(0, viewport.width(), 0, viewport.height(), 255, 0, 0);
}
