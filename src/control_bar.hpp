#pragma once

#include "widget.hpp"

class ControlBar : public Widget {
SHARED_ONLY_CLASS(ControlBar);
public:
    static constexpr int Height = 27;

    ControlBar(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler);

private:
    virtual void widgetRender_() override;
};
