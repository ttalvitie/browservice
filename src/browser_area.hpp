#pragma once

#include "widget.hpp"

class BrowserArea : public Widget {
SHARED_ONLY_CLASS(BrowserArea);
public:
    BrowserArea(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler);

private:
    virtual void widgetRender_() override;
};
