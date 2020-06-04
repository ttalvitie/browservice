#pragma once

#include "widget.hpp"

typedef pair<ImageSlice, ImageSlice> MenuButtonIcon;

class MenuButton;

class MenuButtonEventHandler {
public:
    virtual void onMenuButtonPressed(weak_ptr<MenuButton> button) = 0;

    // Some event forwarding functions useful for FindBar (if we need
    // more of these, we should consider implementing event bubbling)
    virtual void onMenuButtonEnterKeyDown() {}
    virtual void onMenuButtonEscKeyDown() {}
};

class MenuButton :
    public Widget,
    public enable_shared_from_this<MenuButton>
{
SHARED_ONLY_CLASS(MenuButton);
public:
    MenuButton(CKey,
        MenuButtonIcon icon,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<MenuButtonEventHandler> eventHandler
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
    virtual void widgetKeyDownEvent_(int key) override;

    MenuButtonIcon icon_;

    weak_ptr<MenuButtonEventHandler> eventHandler_;

    bool mouseOver_;
    bool mouseDown_;
};
