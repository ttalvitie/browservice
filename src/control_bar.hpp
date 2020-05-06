#pragma once

#include "widget.hpp"

enum class SecurityStatus {
    Secure,
    Warning,
    Insecure
};

class TextField;
class TextLayout;

class ControlBar : public Widget {
SHARED_ONLY_CLASS(ControlBar);
public:
    static constexpr int Height = 27;

    ControlBar(CKey, weak_ptr<WidgetParent> widgetParent);

    void setSecurityStatus(SecurityStatus value);
    void setAddress(const string& addr);

private:
    void afterConstruct_(shared_ptr<ControlBar> self);

    class Layout;
    Layout layout_();

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;

    shared_ptr<TextLayout> addrText_;
    shared_ptr<TextField> addrField_;

    SecurityStatus securityStatus_;
};
