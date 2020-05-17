#pragma once

#include "text_field.hpp"
#include "widget.hpp"

class QualitySelectorEventHandler {
public:
    virtual void onQualityChanged(int quality) = 0;
};

class QualitySelector :
    public Widget,
    public TextFieldEventHandler,
    public enable_shared_from_this<QualitySelector>
{
SHARED_ONLY_CLASS(QualitySelector);
public:
    static constexpr int Width = 48;
    static constexpr int Height = 22;

    QualitySelector(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<QualitySelectorEventHandler> eventHandler,
        bool allowPNG
    );

    // TextFieldEventHandler:
    virtual void onTextFieldSubmitted(string text) override;
    virtual void onTextFieldLostFocusAfterEdit() override;
    virtual void onTextFieldUDKeyDown(bool down) override;
    virtual void onTextFieldUDKeyUp(bool down) override;
    virtual void onTextFieldWheelEvent(int delta) override;

private:
    void afterConstruct_(shared_ptr<QualitySelector> self);

    void setQuality_(string qualityStr);
    void setQuality_(int quality);

    void updateTextField_();

    void mouseRepeat_(int direction, bool first);

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;
    virtual void widgetMouseDownEvent_(int x, int y, int button) override;
    virtual void widgetMouseUpEvent_(int x, int y, int button) override;
    virtual void widgetMouseWheelEvent_(int x, int y, int delta) override;
    virtual void widgetKeyDownEvent_(int key) override;
    virtual void widgetKeyUpEvent_(int key) override;
    virtual void widgetGainFocusEvent_(int x, int y) override;
    virtual void widgetLoseFocusEvent_() override;

    weak_ptr<QualitySelectorEventHandler> eventHandler_;

    bool allowPNG_;

    shared_ptr<TextField> textField_;

    shared_ptr<Timeout> longMouseRepeatTimeout_;
    shared_ptr<Timeout> shortMouseRepeatTimeout_;

    int quality_;

    bool hasFocus_;
    bool upKeyPressed_;
    bool downKeyPressed_;
    bool upButtonPressed_;
    bool downButtonPressed_;
};
