#pragma once

#include "text_field.hpp"
#include "menu_button.hpp"

class FindBarEventHandler {
public:
    virtual void onFindBarClose() = 0;
};

class FindBar :
    public Widget,
    public TextFieldEventHandler,
    public MenuButtonEventHandler
{
SHARED_ONLY_CLASS(FindBar);
public:
    static constexpr int Width = 180;
    static constexpr int Height = 22;

    FindBar(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<FindBarEventHandler> eventHandler
    );

    // MenuButtonEventHandler:
    virtual void onMenuButtonPressed(weak_ptr<MenuButton> button) override;

private:
    void afterConstruct_(shared_ptr<FindBar> self);

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;

    weak_ptr<FindBarEventHandler> eventHandler_;

    shared_ptr<TextField> textField_;
    shared_ptr<MenuButton> downButton_;
    shared_ptr<MenuButton> upButton_;
    shared_ptr<MenuButton> closeButton_;
};
