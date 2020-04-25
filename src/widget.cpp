#include "widget.hpp"

Widget::Widget(weak_ptr<WidgetEventHandler> eventHandler) {
    CEF_REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;
    viewDirty_ = false;
}

void Widget::setViewport(ImageSlice viewport) {
    CEF_REQUIRE_UI_THREAD();

    viewport_ = viewport;
    widgetViewportUpdated_();
    signalViewDirty_();
}

ImageSlice Widget::getViewport() {
    CEF_REQUIRE_UI_THREAD();
    return viewport_;
}

void Widget::render() {
    CEF_REQUIRE_UI_THREAD();

    viewDirty_ = false;
    widgetRender_();
}

void Widget::signalViewDirty_() {
    CEF_REQUIRE_UI_THREAD();

    if(!viewDirty_) {
        viewDirty_ = true;
        postTask(eventHandler_, &WidgetEventHandler::onWidgetViewDirty);
    }
}
