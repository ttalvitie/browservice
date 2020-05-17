#pragma once

#include "button.hpp"
#include "go_button.hpp"
#include "text_field.hpp"
#include "quality_selector.hpp"
#include "widget.hpp"

enum class SecurityStatus {
    Secure,
    Warning,
    Insecure
};

class ControlBarEventHandler {
public:
    virtual void onAddressSubmitted(string url) = 0;
    virtual void onQualityChanged(int quality) = 0;
    virtual void onPendingDownloadAccepted() = 0;
};

class TextLayout;
class Timeout;

class ControlBar :
    public Widget,
    public TextFieldEventHandler,
    public GoButtonEventHandler,
    public QualitySelectorEventHandler,
    public ButtonEventHandler,
    public enable_shared_from_this<ControlBar>
{
SHARED_ONLY_CLASS(ControlBar);
public:
    static constexpr int Height = 27;

    ControlBar(CKey,
        weak_ptr<WidgetParent> widgetParent,
        weak_ptr<ControlBarEventHandler> eventHandler,
        bool allowPNG
    );

    void setSecurityStatus(SecurityStatus value);
    void setAddress(string addr);
    void setLoading(bool loading);

    void setPendingDownloadCount(int count);
    void setDownloadProgress(vector<int> progress);

    // TextFieldEventHandler:
    virtual void onTextFieldSubmitted(string text) override;

    // GoButtonEventHandler:
    virtual void onGoButtonPressed() override;

    // QualitySelectorEventHandler:
    virtual void onQualityChanged(int quality) override;

    // ButtonEventHandler:
    virtual void onButtonPressed() override;

private:
    void afterConstruct_(shared_ptr<ControlBar> self);

    bool isDownloadVisible_();

    class Layout;
    Layout layout_();

    // Widget:
    virtual void widgetViewportUpdated_() override;
    virtual void widgetRender_() override;
    virtual vector<shared_ptr<Widget>> widgetListChildren_() override;

    weak_ptr<ControlBarEventHandler> eventHandler_;

    bool allowPNG_;

    shared_ptr<Timeout> animationTimeout_;

    shared_ptr<TextLayout> addrText_;
    shared_ptr<TextField> addrField_;

    SecurityStatus securityStatus_;

    shared_ptr<GoButton> goButton_;

    shared_ptr<TextLayout> qualityText_;
    shared_ptr<QualitySelector> qualitySelector_;

    int pendingDownloadCount_;
    vector<int> downloadProgress_;
    shared_ptr<Button> downloadButton_;

    bool loading_;
    optional<steady_clock::time_point> loadingAnimationStartTime_;
};
