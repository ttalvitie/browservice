#pragma once

#include "widget.hpp"

namespace browservice {

class ControlBar;
class ControlBarEventHandler;
class BrowserAreaEventHandler;
class BrowserArea;

class RootWidget : public Widget {
SHARED_ONLY_CLASS(RootWidget);
public:
    RootWidget(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<ControlBarEventHandler> controlBarEventHandler,
        weak_ptr<BrowserAreaEventHandler> browserAreaEventHandler,
        bool allowPNG
    );

    shared_ptr<ControlBar> controlBar();
    shared_ptr<BrowserArea> browserArea();

private:
    void afterConstruct_(shared_ptr<RootWidget> self);

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;

    bool allowPNG_;

    weak_ptr<ControlBarEventHandler> controlBarEventHandler_;
    weak_ptr<BrowserAreaEventHandler> browserAreaEventHandler_;

    shared_ptr<ControlBar> controlBar_;
    shared_ptr<BrowserArea> browserArea_;
};

}
