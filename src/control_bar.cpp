#include "control_bar.hpp"

#include "text.hpp"
#include "text_field.hpp"

namespace {

ImageSlice createSecureIcon() {
    return ImageSlice::createImageFromStrings({
        "WWWWWBBWWWWWW",
        "WWWWBWWBWWWWW",
        "WWWBWWWWBWWWW",
        "WWWBWWWWBWWWW",
        "WWWBWWWWBWWWW",
        "WEEEEEEEEEBWW",
        "WEGGGGGGGGBWW",
        "WEGGGGGGGGBWW",
        "WEGGGGGGGGBWW",
        "WEGGGGGGGGBWW",
        "WEGGGGGGGGBWW",
        "WBBBBBBBBBBWW",
        "WWWWWWWWWWWWW",
    }, {
        {'B', {0, 0, 0}},
        {'E', {128, 128, 128}},
        {'G', {192, 192, 192}},
        {'W', {255, 255, 255}},
    });
}
ImageSlice createWarningIcon() {
    return ImageSlice::createImageFromStrings({
        "WWWWWBBWvWWWW",
        "WWWWBWWvYjWWW",
        "WWWBWWWvYjWWW",
        "WWWBWWvYYYjWW",
        "WWWBWWvYBYjWW",
        "WEEEEEvYBYjWW",
        "WEGGGvYYBYYjW",
        "WEGGGvYYBYYjW",
        "WEGGGvYYBYYjW",
        "WEGGvYYYYYYYj",
        "WEGGvYYYBYYYj",
        "WBBBvYYYYYYYj",
        "WWWWyyyyyyyyy",
    }, {
        {'B', {0, 0, 0}},
        {'E', {128, 128, 128}},
        {'G', {192, 192, 192}},
        {'W', {255, 255, 255}},
        {'Y', {255, 255, 0}},
        {'y', {32, 32, 0}},
        {'j', {64, 64, 0}},
        {'v', {128, 128, 0}},
    });
}
ImageSlice createInsecureIcon() {
    return ImageSlice::createImageFromStrings({
        "WWWWWBBWWWRRW",
        "WWWWBWWBWRRRW",
        "WWWBWWWWRRRWW",
        "WWWBWWWRRRWWW",
        "WWWBWWRRRWWWW",
        "WEEEERRRWbBWW",
        "WEGGRRRWgGBWW",
        "WEGRRRWgGGBWW",
        "WERRRWgGGGBWW",
        "WRRRWgGGGGBWW",
        "RRRWgGGGGGBWW",
        "RRWbBBBBBBBWW",
        "WWWWWWWWWWWWW",
    }, {
        {'B', {0, 0, 0}},
        {'b', {128, 128, 128}},
        {'E', {128, 128, 128}},
        {'G', {192, 192, 192}},
        {'g', {224, 224, 224}},
        {'W', {255, 255, 255}},
        {'R', {255, 0, 0}},
    });
}

ImageSlice secureIcon = createSecureIcon();
ImageSlice warningIcon = createWarningIcon();
ImageSlice insecureIcon = createInsecureIcon();

ImageSlice securityStatusIcon(SecurityStatus status) {
    if(status == SecurityStatus::Secure) {
        return secureIcon;
    } else if(status == SecurityStatus::Warning) {
        return warningIcon;
    } else if(status == SecurityStatus::Insecure) {
        return insecureIcon;
    }
    CHECK(false);
    return insecureIcon;
}

}

struct ControlBar::Layout {
    Layout(int width) : width(width) {
        int contentStart = 1;
        int contentEnd = width - 1;

        int addrStart = contentStart;
        int addrEnd = contentEnd;

        addrTextStart = addrStart;
        addrTextEnd = addrTextStart + 52;

        addrBoxStart = addrTextEnd;
        addrBoxEnd = addrEnd;

        int addrBoxInnerStart = addrBoxStart + 2;
        int addrBoxInnerEnd = addrBoxEnd - 2;

        securityIconStart = addrBoxInnerStart + 4;
        int securityIconEnd = securityIconStart + 13;

        addrFieldStart = securityIconEnd + 4;
        addrFieldEnd = addrBoxInnerEnd;
    }

    int width;

    int addrTextStart;
    int addrTextEnd;

    int addrBoxStart;
    int addrBoxEnd;

    int securityIconStart;

    int addrFieldStart;
    int addrFieldEnd;
};

ControlBar::ControlBar(CKey, weak_ptr<WidgetParent> widgetParent)
    : Widget(widgetParent)
{
    CEF_REQUIRE_UI_THREAD();

    addrText_ = TextLayout::create();
    addrText_->setText("Address");

    securityStatus_ = SecurityStatus::Insecure;

    // Initialization is completed in afterConstruct_
}

void ControlBar::setSecurityStatus(SecurityStatus value) {
    CEF_REQUIRE_UI_THREAD();

    if(securityStatus_ != value) {
        securityStatus_ = value;
        signalViewDirty_();
    }
}

void ControlBar::setAddress(const string& addr) {
    CEF_REQUIRE_UI_THREAD();
    addrField_->setText(addr);
}

void ControlBar::afterConstruct_(shared_ptr<ControlBar> self) {
    addrField_ = TextField::create(self);
}

ControlBar::Layout ControlBar::layout_() {
    return Layout(getViewport().width());
}

void ControlBar::widgetViewportUpdated_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    Layout layout = layout_();

    addrField_->setViewport(viewport.subRect(
        layout.addrFieldStart, layout.addrFieldEnd, 4, viewport.height()
    ));
}

void ControlBar::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    Layout layout = layout_();

    // Frame
    viewport.fill(0, layout.width - 1, 0, 1, 255);
    viewport.fill(0, 1, 1, Height - 4, 255);
    viewport.fill(layout.width - 1, layout.width, 0, Height - 3, 128);
    viewport.fill(0, layout.width - 1, Height - 4, Height - 3, 128);
    viewport.fill(0, layout.width, Height - 3, Height - 2, 255);
    viewport.fill(0, layout.width, Height - 2, Height - 1, 128);
    viewport.fill(0, layout.width, Height - 1, Height, 0, 0, 0);

    // Background
    viewport.fill(1, layout.width - 1, 1, Height - 4, 192);

    // "Address" text
    addrText_->render(
        viewport.subRect(layout.addrTextStart, layout.addrTextEnd, 1, Height - 4),
        3, 3
    );

    // Address field frame
    viewport.fill(layout.addrBoxStart, layout.addrBoxEnd - 1, 1, 2, 128);
    viewport.fill(layout.addrBoxStart, layout.addrBoxStart + 1, 2, Height - 5, 128);
    viewport.fill(layout.addrBoxEnd - 1, layout.addrBoxEnd, 1, Height - 4, 255);
    viewport.fill(layout.addrBoxStart, layout.addrBoxEnd - 1, Height - 5, Height - 4, 255);
    viewport.fill(layout.addrBoxStart + 1, layout.addrBoxEnd - 2, 2, 3, 0);
    viewport.fill(layout.addrBoxStart + 1, layout.addrBoxStart + 2, 3, Height - 6, 0);
    viewport.fill(layout.addrBoxEnd - 2, layout.addrBoxEnd - 1, 2, Height - 5, 192);
    viewport.fill(layout.addrBoxStart + 1, layout.addrBoxEnd - 2, Height - 6, Height - 5, 192);

    // Address field background
    viewport.fill(layout.addrBoxStart + 2, layout.addrBoxEnd - 2, 3, Height - 6, 255);

    // Security icon
    viewport.putImage(securityStatusIcon(securityStatus_), layout.securityIconStart, 6);
}

vector<shared_ptr<Widget>> ControlBar::widgetListChildren_() {
    CEF_REQUIRE_UI_THREAD();
    return {addrField_};
}
