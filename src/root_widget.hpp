#pragma once

#include "widget.hpp"

class ControlBar;
class BrowserArea;

class RootWidget :
    public Widget,
    public WidgetEventHandler
{
SHARED_ONLY_CLASS(RootWidget);
public:
    RootWidget(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler);

    // WidgetEventHandler:
    virtual void onWidgetViewDirty() override;

private:
    void afterConstruct_(shared_ptr<RootWidget> self);

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;

    shared_ptr<ControlBar> controlBar_;
    shared_ptr<BrowserArea> browserArea_;
};
