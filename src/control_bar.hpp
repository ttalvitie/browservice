#pragma once

#include "widget.hpp"

class ControlBar : public Widget {
SHARED_ONLY_CLASS(ControlBar);
public:
    static constexpr int Height = 27;

    ControlBar(CKey, weak_ptr<WidgetParent> widgetParent);

private:
    virtual void widgetMouseDownEvent_(int x, int y, int button) override {
        LOG(INFO) << "Control bar: Mouse button " << button << " down at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseUpEvent_(int x, int y, int button) override {
        LOG(INFO) << "Control bar: Mouse button " << button << " up at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseDoubleClickEvent_(int x, int y) override {
        LOG(INFO) << "Control bar: Mouse doubleclick at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseWheelEvent_(int x, int y, int delta) override {
        LOG(INFO) << "Control bar: Mouse wheel delta " << delta << " at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseMoveEvent_(int x, int y) override {
        LOG(INFO) << "Control bar: Mouse move at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseEnterEvent_(int x, int y) override {
        LOG(INFO) << "Control bar: Mouse enter at (" << x << ", " << y << ")\n";
    }
    virtual void widgetMouseLeaveEvent_(int x, int y) override {
        LOG(INFO) << "Control bar: Mouse leave at (" << x << ", " << y << ")\n";
    }
    virtual void widgetKeyDownEvent_(Key key) override {
        LOG(INFO) << "Control bar: Key " << key.name() << " down\n";
    }
    virtual void widgetKeyUpEvent_(Key key) override {
        LOG(INFO) << "Control bar: Key " << key.name() << " up\n";
    }
    virtual void widgetGainFocusEvent_(int x, int y) override {
        LOG(INFO) << "Control bar: Gain focus at (" << x << ", " << y << ")\n";
    }
    virtual void widgetLoseFocusEvent_() override {
        LOG(INFO) << "Control bar: Lose focus\n";
    }

    virtual void widgetRender_() override;
};
