#pragma once

#include "widget.hpp"

class OverflowTextLayout;
class Timeout;

class TextField :
    public Widget,
    public enable_shared_from_this<TextField>
{
SHARED_ONLY_CLASS(TextField);
public:
    TextField(CKey, weak_ptr<WidgetParent> widgetParent);

    void setText(const string& text);

private:
    void unsetCaret_();
    void setCaret_(int start, int end);
    void scheduleBlinkCaret_();

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual void widgetMouseDownEvent_(int x, int y, int button) override;
    virtual void widgetMouseUpEvent_(int x, int y, int button) override;
    virtual void widgetMouseDoubleClickEvent_(int x, int y) override;
    virtual void widgetMouseMoveEvent_(int x, int y) override;
    virtual void widgetKeyDownEvent_(Key key) override;
    virtual void widgetKeyUpEvent_(Key key) override;
    virtual void widgetLoseFocusEvent_() override;

    shared_ptr<OverflowTextLayout> textLayout_;

    bool leftMouseButtonDown_;
    bool shiftKeyDown_;

    bool caretActive_;
    int caretStart_;
    int caretEnd_;
    bool caretBlinkState_;

    shared_ptr<Timeout> caretBlinkTimeout_;
};
