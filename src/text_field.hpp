#pragma once

#include "widget.hpp"

class TextFieldEventHandler {
public:
    virtual void onTextFieldSubmitted(string text) {}
    virtual void onTextFieldLostFocusAfterEdit() {}
    virtual void onTextFieldTextChanged() {}

    // Some event forwarding functions useful for QualitySelector (if we need
    // more of these, we should consider implementing event bubbling)
    virtual void onTextFieldUDKeyDown(bool down) {}
    virtual void onTextFieldEscKeyDown() {}
    virtual void onTextFieldUDKeyUp(bool down) {}
    virtual void onTextFieldWheelEvent(int delta) {}
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

    bool hasFocus();

    void setRemoveCaretOnSubmit(bool value);

private:
    void unsetCaret_();
    void setCaret_(int start, int end);
    void scheduleBlinkCaret_();

    void typeText_(const char* textPtr, int textLength);
    void typeCharacter_(int key);
    void eraseRange_();

    void pasteFromClipboard_();
    void copyToClipboard_();

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual void widgetMouseDownEvent_(int x, int y, int button) override;
    virtual void widgetMouseUpEvent_(int x, int y, int button) override;
    virtual void widgetMouseDoubleClickEvent_(int x, int y) override;
    virtual void widgetMouseWheelEvent_(int x, int y, int delta) override;
    virtual void widgetMouseMoveEvent_(int x, int y) override;
    virtual void widgetKeyDownEvent_(int key) override;
    virtual void widgetKeyUpEvent_(int key) override;
    virtual void widgetGainFocusEvent_(int x, int y) override;
    virtual void widgetLoseFocusEvent_() override;

    weak_ptr<TextFieldEventHandler> eventHandler_;

    shared_ptr<OverflowTextLayout> textLayout_;

    bool removeCaretOnSubmit_;

    bool hasFocus_;
    bool leftMouseButtonDown_;
    bool shiftKeyDown_;
    bool controlKeyDown_;

    bool caretActive_;
    int caretStart_;
    int caretEnd_;
    bool caretBlinkState_;

    shared_ptr<Timeout> caretBlinkTimeout_;
};
