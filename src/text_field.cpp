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
