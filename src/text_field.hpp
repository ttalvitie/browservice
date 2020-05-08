#pragma once

#include "widget.hpp"

class TextFieldEventHandler {
public:
    virtual void onTextFieldSubmitted(string text) {}
};

class OverflowTextLayout;
class Timeout;

class TextField :
    public Widget,
    public enable_shared_from_this<TextField>
{
SHARED_ONLY_CLASS(TextField);
public:
    TextField(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<TextFieldEventHandler> eventHandler
    );

    void setText(string text);
    string text();

private:
    void unsetCaret_();
    void setCaret_(int start, int end);
    void scheduleBlinkCaret_();

    void typeCharacter_(string character);
    void eraseRange_();

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

    weak_ptr<TextFieldEventHandler> eventHandler_;

    shared_ptr<OverflowTextLayout> textLayout_;

    bool leftMouseButtonDown_;
    bool shiftKeyDown_;

    bool caretActive_;
    int caretStart_;
    int caretEnd_;
    bool caretBlinkState_;

    shared_ptr<Timeout> caretBlinkTimeout_;
};
