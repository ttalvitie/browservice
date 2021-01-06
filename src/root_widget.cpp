#include "root_widget.hpp"

#include "control_bar.hpp"
#include "browser_area.hpp"

namespace browservice {

RootWidget::RootWidget(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<ControlBarEventHandler> controlBarEventHandler,
    weak_ptr<BrowserAreaEventHandler> browserAreaEventHandler,
    bool allowPNG
)
    : Widget(widgetParent)
{
    REQUIRE_UI_THREAD();

    allowPNG_ = allowPNG;

    controlBarEventHandler_ = controlBarEventHandler;
    browserAreaEventHandler_ = browserAreaEventHandler;

    // Initialization is finalized in afterConstruct_
}

shared_ptr<ControlBar> RootWidget::controlBar() {
    REQUIRE_UI_THREAD();
    return controlBar_;
}

shared_ptr<BrowserArea> RootWidget::browserArea() {
    REQUIRE_UI_THREAD();
    return browserArea_;
}

void RootWidget::afterConstruct_(shared_ptr<RootWidget> self) {
    controlBar_ = ControlBar::create(self, controlBarEventHandler_, allowPNG_);
    browserArea_ = BrowserArea::create(self, browserAreaEventHandler_);

    controlBarEventHandler_.reset();
    browserAreaEventHandler_.reset();
}

void RootWidget::widgetViewportUpdated_() {
    REQUIRE_UI_THREAD();

    ImageSlice controlBarViewport, browserAreaViewport;
    tie(controlBarViewport, browserAreaViewport) =
        getViewport().splitY(ControlBar::Height);

    controlBar_->setViewport(controlBarViewport);
    browserArea_->setViewport(browserAreaViewport);
}

vector<shared_ptr<Widget>> RootWidget::widgetListChildren_() {
    REQUIRE_UI_THREAD();
    return {controlBar_, browserArea_};
}

}
