#pragma once

#include "widget.hpp"

class ControlBar;
class BrowserAreaEventHandler;
class BrowserArea;

class RootWidget :
    public Widget,
    public WidgetEventHandler
{
SHARED_ONLY_CLASS(RootWidget);
public:
    RootWidget(CKey,
        weak_ptr<WidgetEventHandler> widgetEventHandler,
        weak_ptr<BrowserAreaEventHandler> browserAreaEventHandler
    );

    shared_ptr<ControlBar> controlBar();
    shared_ptr<BrowserArea> browserArea();

    // WidgetEventHandler:
    virtual void onWidgetViewDirty() override;

private:
    void afterConstruct_(shared_ptr<RootWidget> self);

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;

    weak_ptr<BrowserAreaEventHandler> browserAreaEventHandler_;

    shared_ptr<ControlBar> controlBar_;
    shared_ptr<BrowserArea> browserArea_;
};
