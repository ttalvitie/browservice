#include "text_field.hpp"

#include "text.hpp"

TextField::TextField(CKey, weak_ptr<WidgetParent> widgetParent)
    : Widget(widgetParent)
{
    CEF_REQUIRE_UI_THREAD();
    textLayout_ = OverflowTextLayout::create();
}

void TextField::setText(const string& text) {
    CEF_REQUIRE_UI_THREAD();

    textLayout_->setText(text);
    signalViewDirty_();
}

void TextField::widgetViewportUpdated_() {
    CEF_REQUIRE_UI_THREAD();
    textLayout_->setWidth(getViewport().width());
}

void TextField::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    textLayout_->render(viewport);
}

// For testing, TODO remove
void TextField::widgetMouseDownEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();
    textLayout_->setOffset(x - 50);
    signalViewDirty_();
}
void TextField::widgetMouseMoveEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    int idx = textLayout_->xCoordToIndex(x);
    LOG(INFO) << idx << ": " << x << " -> " << textLayout_->indexToXCoord(idx);
}
