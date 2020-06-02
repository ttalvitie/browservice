#pragma once

#include "text_field.hpp"
#include "menu_button.hpp"

class FindBarEventHandler {
public:
    virtual void onFindBarClose() = 0;
    virtual void onFind(string text, bool forward, bool findNext) = 0;
    virtual void onStopFind(bool clearSelection) = 0;
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

    void open();
    void close();
    void findNext();

    // TextFieldEventHandler:
    virtual void onTextFieldTextChanged() override;
    virtual void onTextFieldSubmitted(string text) override;
    virtual void onTextFieldEscKeyDown() override;

    // MenuButtonEventHandler:
    virtual void onMenuButtonPressed(weak_ptr<MenuButton> button) override;
    virtual void onMenuButtonEnterKeyDown() override;
    virtual void onMenuButtonEscKeyDown() override;

private:
    void afterConstruct_(shared_ptr<FindBar> self);

    bool updateText_(string text);
    void find_(string text, bool forward);

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;

    weak_ptr<FindBarEventHandler> eventHandler_;

    shared_ptr<TextField> textField_;
    shared_ptr<MenuButton> downButton_;
    shared_ptr<MenuButton> upButton_;
    shared_ptr<MenuButton> closeButton_;

    bool isOpen_;
    optional<string> text_;
    bool lastDirForward_;
};
