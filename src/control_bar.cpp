#include "control_bar.hpp"

#include "text.hpp"
#include "timeout.hpp"

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
    Layout(int width, bool isDownloadVisible) : width(width) {
        int contentStart = 1;
        int contentEnd = width - 1;

        const int SeparatorWidth = 4;
        const int AddressTextWidth = 52;
        const int QualityTextWidth = 46;

        int downloadWidth = isDownloadVisible ? 98 : 0;
        int downloadSpacerWidth = isDownloadVisible ? 2 : 0;

        downloadEnd = contentEnd;
        downloadStart = downloadEnd - downloadWidth;

        int downloadSpacerEnd = downloadStart;
        int downloadSpacerStart = downloadSpacerEnd - downloadSpacerWidth;

        qualitySelectorEnd = downloadSpacerStart;
        qualitySelectorStart = qualitySelectorEnd - QualitySelector::Width;

        qualityTextEnd = qualitySelectorStart;
        qualityTextStart = qualityTextEnd - QualityTextWidth;

        int separatorEnd = qualityTextStart;
        int separatorStart = separatorEnd - SeparatorWidth;
        separatorPos = separatorStart + SeparatorWidth / 2;

        int addrStart = contentStart;
        int addrEnd = separatorStart;

        addrTextStart = addrStart;
        addrTextEnd = addrTextStart + AddressTextWidth;

        goButtonEnd = addrEnd;
        goButtonStart = goButtonEnd - MenuButton::Width;

        addrBoxStart = addrTextEnd;
        addrBoxEnd = goButtonStart - 1;

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

    int goButtonStart;
    int goButtonEnd;

    int securityIconStart;

    int addrFieldStart;
    int addrFieldEnd;

    int separatorPos;

    int qualityTextStart;
    int qualityTextEnd;

    int qualitySelectorStart;
    int qualitySelectorEnd;

    int downloadStart;
    int downloadEnd;
};

ControlBar::ControlBar(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<ControlBarEventHandler> eventHandler,
    bool allowPNG
)
    : Widget(widgetParent)
{
    requireUIThread();

    eventHandler_ = eventHandler;

    allowPNG_ = allowPNG;

    animationTimeout_ = Timeout::create(30);

    addrText_ = TextLayout::create();
    addrText_->setText("Address");

    qualityText_ = TextLayout::create();
    qualityText_->setText("Quality");

    securityStatus_ = SecurityStatus::Insecure;

    pendingDownloadCount_ = 0;

    loading_ = false;

    // Initialization is completed in afterConstruct_
}

void ControlBar::setSecurityStatus(SecurityStatus value) {
    requireUIThread();

    if(securityStatus_ != value) {
        securityStatus_ = value;
        signalViewDirty_();
    }
}

void ControlBar::setAddress(string addr) {
    requireUIThread();
    addrField_->setText(move(addr));
}

void ControlBar::setLoading(bool loading) {
    requireUIThread();

    if(loading != loading_) {
        loading_ = loading;
        signalViewDirty_();
    }
}

void ControlBar::setPendingDownloadCount(int count) {
    requireUIThread();
    CHECK(count >= 0);

    if(count != pendingDownloadCount_) {
        pendingDownloadCount_ = count;

        if(count > 0) {
            downloadButton_->setEnabled(true);
            downloadButton_->setText("Download (" + toString(count) + ")");
        } else {
            downloadButton_->setEnabled(false);
            downloadButton_->setText("Download");
        }

        widgetViewportUpdated_();
        signalViewDirty_();
    }
}

void ControlBar::setDownloadProgress(vector<int> progress) {
    requireUIThread();

    if(progress != downloadProgress_) {
        downloadProgress_ = move(progress);
        signalViewDirty_();
        widgetViewportUpdated_();
    }
}

void ControlBar::onTextFieldSubmitted(string text) {
    requireUIThread();
    postTask(eventHandler_, &ControlBarEventHandler::onAddressSubmitted, text);
}

void ControlBar::onMenuButtonPressed(weak_ptr<MenuButton> button) {
    requireUIThread();

    if(button.lock() == goButton_) {
        postTask(
            eventHandler_,
            &ControlBarEventHandler::onAddressSubmitted,
            addrField_->text()
        );
    }
}

void ControlBar::onQualityChanged(int quality) {
    requireUIThread();
    postTask(eventHandler_, &ControlBarEventHandler::onQualityChanged, quality);
}

void ControlBar::onButtonPressed() {
    requireUIThread();
    postTask(eventHandler_, &ControlBarEventHandler::onPendingDownloadAccepted);
}

void ControlBar::afterConstruct_(shared_ptr<ControlBar> self) {
    addrField_ = TextField::create(self, self);
    goButton_ = MenuButton::create(GoIcon, self, self);
    qualitySelector_ = QualitySelector::create(self, self, allowPNG_);
    downloadButton_ = Button::create(self, self);
}

bool ControlBar::isDownloadVisible_() {
    return pendingDownloadCount_ > 0 || !downloadProgress_.empty();
}

ControlBar::Layout ControlBar::layout_() {
    return Layout(getViewport().width(), isDownloadVisible_());
}

void ControlBar::widgetViewportUpdated_() {
    requireUIThread();

    ImageSlice viewport = getViewport();
    Layout layout = layout_();

    addrField_->setViewport(viewport.subRect(
        layout.addrFieldStart, layout.addrFieldEnd, 3, Height - 8
    ));
    goButton_->setViewport(viewport.subRect(
        layout.goButtonStart, layout.goButtonEnd, 1, Height - 4
    ));
    qualitySelector_->setViewport(viewport.subRect(
        layout.qualitySelectorStart, layout.qualitySelectorEnd, 1, Height - 4
    ));
    if(isDownloadVisible_()) {
        downloadButton_->setViewport(viewport.subRect(
            layout.downloadStart, layout.downloadEnd,
            1, Height - 4 - (downloadProgress_.empty() ? 0 : 5)
        ));
    }
}

void ControlBar::widgetRender_() {
    requireUIThread();

    animationTimeout_->clear(false);

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
        3, -4
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

    // Separator
    viewport.fill(layout.separatorPos - 1, layout.separatorPos, 1, Height - 4, 128);
    viewport.fill(layout.separatorPos, layout.separatorPos + 1, 1, Height - 4, 255);

    // "Quality" text
    qualityText_->render(
        viewport.subRect(layout.qualityTextStart, layout.qualityTextEnd, 1, Height - 4),
        3, -4
    );

    // Download progress bar
    if(!downloadProgress_.empty()) {
        int startY = Height - 9;
        int endY = Height - 4;

        int startX = layout.downloadStart;
        int downloadCount = (int)downloadProgress_.size();
        int rangeLength = layout.downloadEnd - layout.downloadStart;
        for(int i = 0; i < downloadCount; ++i) {
            int endX =
                layout.downloadStart +
                ((i + 1) * rangeLength + downloadCount - 1) / downloadCount;
            int barMaxX = endX - (i == downloadCount - 1 ? 0 : 1);
            int barEndX = startX + downloadProgress_[i] * (barMaxX - startX) / 100;
            viewport.fill(startX, barEndX, startY, endY, 0, 0, 255);
            viewport.fill(barEndX, endX, startY, endY, 192);

            startX = endX;
        }
    }

    // Loading bar
    if(loading_) {
        int64_t elapsed;
        if(loadingAnimationStartTime_) {
            elapsed = duration_cast<milliseconds>(
                steady_clock::now() - *loadingAnimationStartTime_
            ).count();
        } else {
            elapsed = 0;
            loadingAnimationStartTime_ = steady_clock::now();
        }

        ImageSlice viewport = getViewport();
        Layout layout = layout_();
        ImageSlice slice = viewport.subRect(
            layout.addrFieldStart - 2, layout.addrBoxEnd - 2,
            Height - 7, Height - 6
        );

        if(!slice.isEmpty()) {
            int barWidth = slice.width() / 12;
            int64_t p = elapsed * (int64_t)slice.width() / (int64_t)5000;
            int barStart = p % slice.width();
            int barEnd = (p + barWidth) % slice.width();

            if(barStart <= barEnd) {
                slice.fill(barStart, barEnd, 0, slice.height(), 0, 0, 255);
            } else {
                slice.fill(0, barEnd, 0, slice.height(), 0, 0, 255);
                slice.fill(barStart, slice.width(), 0, slice.height(), 0, 0, 255);
            }
        }

        weak_ptr<ControlBar> selfWeak = shared_from_this();
        animationTimeout_->set([selfWeak]() {
            if(shared_ptr<ControlBar> self = selfWeak.lock()) {
                self->signalViewDirty_();
            }
        });
    } else {
        loadingAnimationStartTime_.reset();
    }
}

vector<shared_ptr<Widget>> ControlBar::widgetListChildren_() {
    requireUIThread();
    vector<shared_ptr<Widget>> children =
        {addrField_, goButton_, qualitySelector_};
    if(isDownloadVisible_()) {
        children.push_back(downloadButton_);
    }
    return children;
}
