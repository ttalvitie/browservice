#pragma once

#include "widget.hpp"

class OverflowTextLayout;

class TextField : public Widget {
SHARED_ONLY_CLASS(TextField);
public:
    TextField(CKey, weak_ptr<WidgetParent> widgetParent);

    void setText(const string& text);

private:
    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual void widgetMouseDownEvent_(int x, int y, int button) override;
    virtual void widgetMouseMoveEvent_(int x, int y) override;

    shared_ptr<OverflowTextLayout> textLayout_;
};
