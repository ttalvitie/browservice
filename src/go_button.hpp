#include "widget.hpp"

class GoButtonEventHandler {
public:
    virtual void onGoButtonPressed() = 0;
};

class GoButton : public Widget {
SHARED_ONLY_CLASS(GoButton);
public:
    static constexpr int Width = 22;
    static constexpr int Height = 22;

    GoButton(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<GoButtonEventHandler> eventHandler
    );

private:
    void mouseMove_(int x, int y);

    // Widget:
    virtual void widgetRender_() override;
    virtual void widgetMouseDownEvent_(int x, int y, int button) override;
    virtual void widgetMouseUpEvent_(int x, int y, int button) override;
    virtual void widgetMouseMoveEvent_(int x, int y) override;
    virtual void widgetMouseEnterEvent_(int x, int y) override;
    virtual void widgetMouseLeaveEvent_(int x, int y) override;

    weak_ptr<GoButtonEventHandler> eventHandler_;

    bool mouseOver_;
    bool mouseDown_;
};
