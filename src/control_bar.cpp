#include "control_bar.hpp"

ControlBar::ControlBar(CKey, weak_ptr<WidgetParent> widgetParent)
    : Widget(widgetParent)
{
    CEF_REQUIRE_UI_THREAD();
}

void ControlBar::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    viewport.fill(0, viewport.width(), 0, viewport.height(), 0, 0, 255);
}
