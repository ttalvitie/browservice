#pragma once

#include "widget.hpp"

class ControlBar : public Widget {
SHARED_ONLY_CLASS(ControlBar);
public:
    static constexpr int Height = 27;

    ControlBar(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler);

    virtual void widgetMouseDownEvent_(int x, int y, int button) override {
        LOG(INFO) << "Mouse button " << button << " down at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseUpEvent_(int x, int y, int button) override {
        LOG(INFO) << "Mouse button " << button << " up at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseDoubleClickEvent_(int x, int y) override {
        LOG(INFO) << "Mouse doubleclick at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseWheelEvent_(int x, int y, int delta) override {
        LOG(INFO) << "Mouse wheel delta " << delta << " at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseMoveEvent_(int x, int y) override {
        LOG(INFO) << "Mouse move at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseEnterEvent_(int x, int y) override {
        LOG(INFO) << "Mouse enter at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseLeaveEvent_(int x, int y) override {
        LOG(INFO) << "Mouse leave at (" << x << ", " << y << ")\n";
    }
    virtual void widgetKeyDownEvent_(Key key) override {
        LOG(INFO) << "Key " << key.name() << " down\n";
    }
    virtual void widgetKeyUpEvent_(Key key) override {
        LOG(INFO) << "Key " << key.name() << " up\n";
    }
    virtual void widgetGainFocusEvent_(int x, int y) override {
        LOG(INFO) << "Gain focus at (" << x << ", " << y << ")\n";
    }
    virtual void widgetLoseFocusEvent_() override {
        LOG(INFO) << "Lose focus\n";
    }

private:
    virtual void widgetRender_() override;
};
