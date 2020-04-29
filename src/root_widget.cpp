#include "root_widget.hpp"

#include "control_bar.hpp"
#include "browser_area.hpp"

RootWidget::RootWidget(CKey,
    weak_ptr<WidgetEventHandler> widgetEventHandler,
    weak_ptr<BrowserAreaEventHandler> browserAreaEventHandler
)
    : Widget(widgetEventHandler),
      browserAreaEventHandler_(browserAreaEventHandler)
{
    CEF_REQUIRE_UI_THREAD();
    // Initialization is finalized in afterConstruct_
}

shared_ptr<ControlBar> RootWidget::controlBar() {
    CEF_REQUIRE_UI_THREAD();
    return controlBar_;
}

shared_ptr<BrowserArea> RootWidget::browserArea() {
    CEF_REQUIRE_UI_THREAD();
    return browserArea_;
}

void RootWidget::onWidgetViewDirty() {
    CEF_REQUIRE_UI_THREAD();
    signalViewDirty_();
}

void RootWidget::afterConstruct_(shared_ptr<RootWidget> self) {
    controlBar_ = ControlBar::create(self);
    browserArea_ = BrowserArea::create(self, browserAreaEventHandler_);
    browserAreaEventHandler_.reset();
}

void RootWidget::widgetViewportUpdated_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice controlBarViewport, browserAreaViewport;
    tie(controlBarViewport, browserAreaViewport) =
        getViewport().splitY(ControlBar::Height);

    controlBar_->setViewport(controlBarViewport);
    browserArea_->setViewport(browserAreaViewport);
}

void RootWidget::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    controlBar_->render();
    browserArea_->render();
}
