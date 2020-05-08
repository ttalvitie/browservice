#pragma once

#include "text_field.hpp"
#include "widget.hpp"

enum class SecurityStatus {
    Secure,
    Warning,
    Insecure
};

class ControlBarEventHandler {
public:
    virtual void onAddressSubmitted(string url) = 0;
};

class TextLayout;

class ControlBar :
    public Widget,
    public TextFieldEventHandler
{
SHARED_ONLY_CLASS(ControlBar);
public:
    static constexpr int Height = 27;

    ControlBar(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<ControlBarEventHandler> eventHandler
    );

    void setSecurityStatus(SecurityStatus value);
    void setAddress(const string& addr);

    // TextFieldEventHandler:
    virtual void onTextFieldSubmitted(string text) override;

private:
    void afterConstruct_(shared_ptr<ControlBar> self);

    class Layout;
    Layout layout_();

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;

    weak_ptr<ControlBarEventHandler> eventHandler_;

    shared_ptr<TextLayout> addrText_;
    shared_ptr<TextField> addrField_;

    SecurityStatus securityStatus_;
};
