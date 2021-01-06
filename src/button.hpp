#include "widget.hpp"

namespace browservice {

class ButtonEventHandler {
public:
    virtual void onButtonPressed() = 0;
};

class TextLayout;

class Button : public Widget {
SHARED_ONLY_CLASS(Button);
public:
    Button(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<ButtonEventHandler> eventHandler
    );

    void setEnabled(bool enabled);
    void setText(string text);

private:
    // Widget:
    virtual void widgetRender_() override;
    virtual void widgetMouseDownEvent_(int x, int y, int button) override;
    virtual void widgetMouseUpEvent_(int x, int y, int button) override;
    virtual void widgetMouseMoveEvent_(int x, int y) override;

    weak_ptr<ButtonEventHandler> eventHandler_;

    bool enabled_;
    bool mouseDown_;
    bool pressed_;

    shared_ptr<TextLayout> textLayout_;
};

}
