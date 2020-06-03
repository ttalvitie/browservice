#pragma once

#include "button.hpp"
#include "find_bar.hpp"
#include "menu_button.hpp"
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
    virtual void onFind(string text, bool forward, bool findNext) = 0;
    virtual void onStopFind(bool clearSelection) = 0;
    virtual void onClipboardButtonPressed() = 0;
};

class TextLayout;
class Timeout;

class ControlBar :
    public Widget,
    public TextFieldEventHandler,
    public MenuButtonEventHandler,
    public QualitySelectorEventHandler,
    public ButtonEventHandler,
    public FindBarEventHandler,
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

    void openFindBar();
    void findNext();

    void activateAddress();

    // TextFieldEventHandler:
    virtual void onTextFieldSubmitted(string text) override;

    // MenuButtonEventHandler:
    virtual void onMenuButtonPressed(weak_ptr<MenuButton> button) override;

    // QualitySelectorEventHandler:
    virtual void onQualityChanged(int quality) override;

    // ButtonEventHandler:
    virtual void onButtonPressed() override;

    // FindBarEventHandler:
    virtual void onFindBarClose() override;
    virtual void onFind(string text, bool forward, bool findNext) override;
    virtual void onStopFind(bool clearSelection) override;

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

    shared_ptr<MenuButton> goButton_;
    shared_ptr<MenuButton> findButton_;
    shared_ptr<MenuButton> clipboardButton_;

    shared_ptr<TextLayout> qualityText_;
    shared_ptr<QualitySelector> qualitySelector_;

    bool findBarVisible_;
    shared_ptr<TextLayout> findText_;
    shared_ptr<FindBar> findBar_;

    int pendingDownloadCount_;
    vector<int> downloadProgress_;
    shared_ptr<Button> downloadButton_;

    bool loading_;
    optional<steady_clock::time_point> loadingAnimationStartTime_;
};
