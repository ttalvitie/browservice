#include "control_bar.hpp"

#include "text.hpp"
#include "timeout.hpp"

namespace browservice {

namespace {

ImageSlice secureIcon = ImageSlice::createImageFromStrings({
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

ImageSlice warningIcon = ImageSlice::createImageFromStrings({
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

ImageSlice insecureIcon = ImageSlice::createImageFromStrings({
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

ImageSlice securityStatusIcon(SecurityStatus status) {
    if(status == SecurityStatus::Secure) {
        return secureIcon;
    } else if(status == SecurityStatus::Warning) {
        return warningIcon;
    } else if(status == SecurityStatus::Insecure) {
        return insecureIcon;
    }
    REQUIRE(false);
    return insecureIcon;
}

vector<string> goIconPattern = {
    "GGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGBGGGGGG",
    "GGGGGGGGGGGGBBGGGGG",
    "GGGGGGGGGGGGBUBGGGG",
    "GGGccccccccccUuBGGG",
    "GGGcUUUUUUUUUUvvBGG",
    "GGGcUvMMMMMMMMMMdBG",
    "GGGcUMMMMMMMMMMddBG",
    "GGGcMDDDDDDDDMdDBGG",
    "GGGcbbbbbbbbbdDBGGG",
    "GGGGGGGGGGGGBDBGGGG",
    "GGGGGGGGGGGGBBGGGGG",
    "GGGGGGGGGGGGBGGGGGG",
    "GGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGG"
};

MenuButtonIcon goIcon = {
    ImageSlice::createImageFromStrings(
        goIconPattern,
        {
            {'G', {192, 192, 192}},
            {'B', {0, 0, 0}},
            {'b', {32, 32, 32}},
            {'c', {64, 64, 64}},
            {'W', {255, 255, 255}},
            {'U', {120, 255, 120}},
            {'u', {109, 236, 109}},
            {'v', {102, 226, 102}},
            {'M', {96, 216, 96}},
            {'d', {82, 188, 82}},
            {'D', {68, 160, 68}},
        }
    ),
    ImageSlice::createImageFromStrings(
        goIconPattern,
        {
            {'G', {192, 192, 192}},
            {'B', {0, 0, 0}},
            {'b', {32, 32, 32}},
            {'c', {64, 64, 64}},
            {'W', {255, 255, 255}},
            {'U', {255, 255, 255}},
            {'u', {232, 232, 232}},
            {'v', {214, 214, 214}},
            {'M', {200, 200, 200}},
            {'d', {172, 172, 172}},
            {'D', {144, 144, 144}},
        }
    )
};

vector<string> openBookmarksIconPattern = {
    "...................",
    "...................",
    "...*************...",
    "...**wwwwwwwwww*...",
    "...*K*ww*******w*..",
    "...*K****rrrrr***..",
    "...*Kkkk*rrrrr*k*..",
    "...*Kkkk*rrrrr*k*..",
    "...*Kk--*rrrrr*k*..",
    "...*Kkkk*rr*rr*k*..",
    "...*Kkkk*r*k*r*k*..",
    "...*Kk--**---**k*..",
    "...*Kkkk*kkkkk*k*..",
    "...*Kkkkkkkkkkkk*..",
    "...*Kk---------k*..",
    "...*Kkkkkkkkkkkk*..",
    "....*kkkkkkkkkkk*..",
    "....*************..",
    "..................."
};

MenuButtonIcon openBookmarksIcon = {
    ImageSlice::createImageFromStrings(
        openBookmarksIconPattern,
        {
            {'.', {192, 192, 192}},
            {'*', {0, 0, 0}},
            {'-', {160, 96, 0}},
            {'w', {255, 255, 255}},
            {'r', {255, 128, 128}},
            {'k', {192, 128, 0}},
            {'K', {128, 64, 0}}
        }
    ),
    ImageSlice::createImageFromStrings(
        openBookmarksIconPattern,
        {
            {'.', {192, 192, 192}},
            {'*', {0, 0, 0}},
            {'-', {128, 128, 128}},
            {'w', {255, 255, 255}},
            {'r', {224, 224, 224}},
            {'k', {160, 160, 160}},
            {'K', {96, 96, 96}}
        }
    )
};

vector<string> findIconPattern = {
    "...................",
    "...................",
    "...................",
    ".....AAA....AAA....",
    ".....A3B....A3B....",
    "....ABBBMMMMMBBB...",
    "....ARQPBXXBRQPB...",
    "....ARQPBXXBRQPB...",
    "...A3RQPBBBBRQP1B..",
    "..ABBBBBB..BBBBBBB.",
    "..A4321B....A4321B.",
    "..A4321B....A4321B.",
    "..A4321B....A4321B.",
    "..A4321B....A4321B.",
    "..A4321B....A4321B.",
    "..ARQP1B....ARQP1B.",
    "..ABBBBB....ABBBBB.",
    "...................",
    "..................."
};

MenuButtonIcon findIcon = {
    ImageSlice::createImageFromStrings(
        findIconPattern,
        {
            {'.', {192, 192, 192}},
            {'A', {0, 0, 96}},
            {'M', {0, 0, 64}},
            {'B', {0, 0, 0}},
            {'1', {68, 68, 164}},
            {'P', {82, 82, 170}},
            {'2', {96, 96, 176}},
            {'Q', {110, 110, 182}},
            {'3', {124, 124, 188}},
            {'R', {138, 138, 194}},
            {'4', {152, 152, 200}},
            {'X', {80, 80, 160}}
        }
    ),
    ImageSlice::createImageFromStrings(
        findIconPattern,
        {
            {'.', {192, 192, 192}},
            {'A', {64, 64, 64}},
            {'M', {48, 48, 48}},
            {'B', {0, 0, 0}},
            {'1', {108, 108, 108}},
            {'P', {122, 122, 122}},
            {'2', {136, 136, 136}},
            {'Q', {150, 150, 150}},
            {'3', {164, 164, 164}},
            {'R', {176, 176, 176}},
            {'4', {188, 188, 188}},
            {'X', {116, 116, 116}}
        }
    )
};

vector<string> clipboardIconPattern = {
    "...................",
    "...................",
    "...eeeeeeBBeeeeee..",
    "..e#####BzyB#####E.",
    "..e####BzyyYB####E.",
    "..e#pppBYYYYBppp#E.",
    "..e#pWWBBBBBBWWP#E.",
    "..e#pWWWWWWWWWWP#E.",
    "..e#pWWWWWWWWWWP#E.",
    "..e#pWWWWWWWWWWP#E.",
    "..e#pWWWWWWWWWWP#E.",
    "..e#pWWWWWWqqqqB#E.",
    "..e#pWWWWWWqwwB##E.",
    "..e#pWWWWWWqwB###E.",
    "..e#pWWWWWWqB####E.",
    "..e#pPPPPPPB#####E.",
    "..e##############E.",
    "...EEEEEEEEEEEEEE..",
    "..................."
};

MenuButtonIcon clipboardIcon = {
    ImageSlice::createImageFromStrings(
        clipboardIconPattern,
        {
            {'.', {192, 192, 192}},
            {'W', {255, 255, 255}},
            {'w', {240, 240, 240}},
            {'B', {0, 0, 0}},
            {'e', {96, 48, 24}},
            {'E', {64, 32, 16}},
            {'p', {96, 64, 32}},
            {'q', {80, 80, 80}},
            {'P', {32, 32, 32}},
            {'Y', {224, 224, 0}},
            {'y', {240, 240, 0}},
            {'z', {255, 255, 0}},
            {'#', {232, 156, 118}}
        }
    ),
    ImageSlice::createImageFromStrings(
        clipboardIconPattern,
        {
            {'.', {192, 192, 192}},
            {'W', {255, 255, 255}},
            {'w', {240, 240, 240}},
            {'B', {0, 0, 0}},
            {'e', {64, 64, 64}},
            {'E', {16, 16, 16}},
            {'p', {96, 96, 96}},
            {'q', {96, 96, 96}},
            {'P', {32, 32, 32}},
            {'Y', {224, 224, 224}},
            {'y', {240, 240, 240}},
            {'z', {255, 255, 255}},
            {'#', {192, 192, 192}}
        }
    )
};

const int BtnWidth = 22;

}

struct ControlBar::Layout {
    Layout(
        int width,
        bool isQualitySelectorVisible,
        bool isClipboardButtonVisible,
        bool isDownloadVisible,
        bool isFindBarVisible
    ) : width(width) {
        int contentStart = 1;
        int contentEnd = width - 1;

        const int SeparatorWidth = 4;
        const int AddressTextWidth = 52;
        const int QualityTextWidth = 46;
        const int FindTextWidth = 29;

        int downloadWidth = isDownloadVisible ? 88 : 0;
        int downloadSpacerWidth;
        if(isDownloadVisible && isQualitySelectorVisible) {
            downloadSpacerWidth = 2;
        } else {
            downloadSpacerWidth = 0;
        }

        int separator3Start;
        int separator3End;
        if(isFindBarVisible) {
            findBarEnd = contentEnd;
            findBarStart = findBarEnd - FindBar::Width;
            findTextEnd = findBarStart;
            findTextStart = findTextEnd - FindTextWidth;
            separator3End = findTextStart;
            separator3Start = separator3End - SeparatorWidth;
            separator3Visible = true;
        } else {
            findTextStart = contentEnd;
            findTextEnd = contentEnd;
            findBarStart = contentEnd;
            findBarEnd = contentEnd;
            separator3Start = contentEnd;
            separator3End = contentEnd;
            separator3Visible = false;
        }

        separator3Pos = separator3Start + SeparatorWidth / 2;

        downloadEnd = separator3Start;
        downloadStart = downloadEnd - downloadWidth;

        int downloadSpacerEnd = downloadStart;
        int downloadSpacerStart = downloadSpacerEnd - downloadSpacerWidth;

        qualitySelectorEnd = downloadSpacerStart;
        if(isQualitySelectorVisible) {
            qualitySelectorStart = qualitySelectorEnd - QualitySelector::Width;
        } else {
            qualitySelectorStart = qualitySelectorEnd;
        }

        qualityTextEnd = qualitySelectorStart;
        if(isQualitySelectorVisible) {
            qualityTextStart = qualityTextEnd - QualityTextWidth;
        } else {
            qualityTextStart = qualityTextEnd;
        }

        int separator2End = qualityTextStart;
        int separator2Start;
        if(isQualitySelectorVisible || isDownloadVisible) {
            separator2Start = separator2End - SeparatorWidth;
            separator2Visible = true;
        } else {
            separator2Start = separator2End;
            separator2Visible = false;
        }
        separator2Pos = separator2Start + SeparatorWidth / 2;

        clipboardButtonEnd = separator2Start;
        if(isClipboardButtonVisible) {
            clipboardButtonStart = clipboardButtonEnd - BtnWidth;
        } else {
            clipboardButtonStart = clipboardButtonEnd;
        }

        findButtonEnd = clipboardButtonStart;
        findButtonStart = findButtonEnd - BtnWidth;

        openBookmarksButtonEnd = findButtonStart;
        openBookmarksButtonStart = openBookmarksButtonEnd - BtnWidth;

        int separator1End = openBookmarksButtonStart;
        int separator1Start = separator1End - SeparatorWidth;
        separator1Pos = separator1Start + SeparatorWidth / 2;

        int addrStart = contentStart;
        int addrEnd = separator1Start;

        addrTextStart = addrStart;
        addrTextEnd = addrTextStart + AddressTextWidth;

        bookmarkToggleButtonEnd = addrEnd;
        bookmarkToggleButtonStart = bookmarkToggleButtonEnd - BtnWidth;

        goButtonEnd = bookmarkToggleButtonStart;
        goButtonStart = goButtonEnd - BtnWidth;

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

    int bookmarkToggleButtonStart;
    int bookmarkToggleButtonEnd;

    int securityIconStart;

    int addrFieldStart;
    int addrFieldEnd;

    int separator1Pos;
    int separator2Pos;
    int separator3Pos;
    bool separator2Visible;
    bool separator3Visible;

    int qualityTextStart;
    int qualityTextEnd;

    int qualitySelectorStart;
    int qualitySelectorEnd;

    int downloadStart;
    int downloadEnd;

    int openBookmarksButtonStart;
    int openBookmarksButtonEnd;

    int findButtonStart;
    int findButtonEnd;

    int clipboardButtonStart;
    int clipboardButtonEnd;

    int findTextStart;
    int findTextEnd;

    int findBarStart;
    int findBarEnd;
};

ControlBar::ControlBar(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<ControlBarEventHandler> eventHandler,
    bool allowPNG
)
    : Widget(widgetParent)
{
    REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;

    clipboardButtonEnabled_ = false;
    allowPNG_ = allowPNG;

    animationTimeout_ = Timeout::create(30);

    addrText_ = TextLayout::create();
    addrText_->setText("Address");

    qualityText_ = TextLayout::create();
    qualityText_->setText("Quality");

    findBarVisible_ = false;
    findText_ = TextLayout::create();
    findText_->setText("Find");

    securityStatus_ = SecurityStatus::Insecure;

    pendingDownloadCount_ = 0;

    loading_ = false;

    isBookmarked_ = false;

    // Initialization is completed in afterConstruct_
}

void ControlBar::enableQualitySelector(
    vector<string> labels,
    size_t choiceIdx
) {
    REQUIRE_UI_THREAD();
    REQUIRE(!qualitySelector_);

    shared_ptr<ControlBar> self = shared_from_this();
    qualitySelector_ = QualitySelector::create(
        self, self, move(labels), choiceIdx
    );
    widgetViewportUpdated_();
    signalViewDirty_();
}

void ControlBar::enableClipboardButton() {
    REQUIRE_UI_THREAD();

    if(!clipboardButtonEnabled_) {
        clipboardButtonEnabled_ = true;

        widgetViewportUpdated_();
        signalViewDirty_();
    }
}

void ControlBar::setSecurityStatus(SecurityStatus value) {
    REQUIRE_UI_THREAD();

    if(securityStatus_ != value) {
        securityStatus_ = value;
        signalViewDirty_();
    }
}

void ControlBar::setAddress(string addr) {
    REQUIRE_UI_THREAD();
    addrField_->setText(move(addr));
}

void ControlBar::setLoading(bool loading) {
    REQUIRE_UI_THREAD();

    if(loading != loading_) {
        loading_ = loading;
        signalViewDirty_();
    }
}

void ControlBar::setPendingDownloadCount(int count) {
    REQUIRE_UI_THREAD();
    REQUIRE(count >= 0);

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
    REQUIRE_UI_THREAD();

    if(progress != downloadProgress_) {
        downloadProgress_ = move(progress);
        signalViewDirty_();
        widgetViewportUpdated_();
    }
}

void ControlBar::openFindBar() {
    REQUIRE_UI_THREAD();

    findBarVisible_ = true;
    findBar_->open();
    findBar_->activate();
    widgetViewportUpdated_();
    signalViewDirty_();
}

void ControlBar::findNext() {
    REQUIRE_UI_THREAD();

    if(findBarVisible_) {
        findBar_->findNext();
    }
}

void ControlBar::setFindResult(bool found) {
    REQUIRE_UI_THREAD();

    if(findBarVisible_) {
        findBar_->setFindResult(found);
    }
}

void ControlBar::activateAddress() {
    REQUIRE_UI_THREAD();
    addrField_->activate();
}

void ControlBar::onTextFieldSubmitted(string text) {
    REQUIRE_UI_THREAD();
    postTask(eventHandler_, &ControlBarEventHandler::onAddressSubmitted, text);
}

void ControlBar::onMenuButtonPressed(weak_ptr<MenuButton> button) {
    REQUIRE_UI_THREAD();

    if(button.lock() == goButton_) {
        postTask(
            eventHandler_,
            &ControlBarEventHandler::onAddressSubmitted,
            addrField_->text()
        );
    }

    if(button.lock() == bookmarkToggleButton_) {
        INFO_LOG("TODO: bookmark toggle");
        isBookmarked_ = !isBookmarked_;
        bookmarkToggleButton_->setIcon(isBookmarked_ ? goIcon : findIcon);
    }

    if(button.lock() == openBookmarksButton_) {
        postTask(
            eventHandler_,
            &ControlBarEventHandler::onOpenBookmarksButtonPressed
        );
    }

    if(button.lock() == findButton_) {
        openFindBar();
    }

    if(button.lock() == clipboardButton_ && clipboardButtonEnabled_) {
        postTask(
            eventHandler_,
            &ControlBarEventHandler::onClipboardButtonPressed
        );
    }
}

void ControlBar::onQualityChanged(size_t idx) {
    REQUIRE_UI_THREAD();
    postTask(eventHandler_, &ControlBarEventHandler::onQualityChanged, idx);
}

void ControlBar::onButtonPressed() {
    REQUIRE_UI_THREAD();
    postTask(eventHandler_, &ControlBarEventHandler::onPendingDownloadAccepted);
}

void ControlBar::onFindBarClose() {
    REQUIRE_UI_THREAD();

    if(findBarVisible_) {
        findBarVisible_ = false;
        widgetViewportUpdated_();
        signalViewDirty_();
    }
}

void ControlBar::onFind(string text, bool forward, bool findNext) {
    REQUIRE_UI_THREAD();

    if(findBarVisible_) {
        postTask(
            eventHandler_,
            &ControlBarEventHandler::onFind,
            move(text),
            forward,
            findNext
        );
    }
}

void ControlBar::onStopFind(bool clearSelection) {
    REQUIRE_UI_THREAD();
    postTask(eventHandler_, &ControlBarEventHandler::onStopFind, clearSelection);
}

void ControlBar::afterConstruct_(shared_ptr<ControlBar> self) {
    addrField_ = TextField::create(self, self);
    addrField_->setAllowEmptySubmit(false);

    goButton_ = MenuButton::create(goIcon, self, self);
    bookmarkToggleButton_ = MenuButton::create(isBookmarked_ ? goIcon : findIcon, self, self);
    openBookmarksButton_ = MenuButton::create(openBookmarksIcon, self, self);
    findButton_ = MenuButton::create(findIcon, self, self);
    clipboardButton_ = MenuButton::create(clipboardIcon, self, self);
    downloadButton_ = Button::create(self, self);
    findBar_ = FindBar::create(self, self);
}

bool ControlBar::isDownloadVisible_() {
    return pendingDownloadCount_ > 0 || !downloadProgress_.empty();
}

ControlBar::Layout ControlBar::layout_() {
    return Layout(
        getViewport().width(),
        (bool)qualitySelector_,
        clipboardButtonEnabled_,
        isDownloadVisible_(),
        findBarVisible_
    );
}

void ControlBar::widgetViewportUpdated_() {
    REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    Layout layout = layout_();

    addrField_->setViewport(viewport.subRect(
        layout.addrFieldStart, layout.addrFieldEnd, 3, Height - 8
    ));
    goButton_->setViewport(viewport.subRect(
        layout.goButtonStart, layout.goButtonEnd, 1, Height - 4
    ));
    bookmarkToggleButton_->setViewport(viewport.subRect(
        layout.bookmarkToggleButtonStart, layout.bookmarkToggleButtonEnd, 1, Height - 4
    ));
    openBookmarksButton_->setViewport(viewport.subRect(
        layout.openBookmarksButtonStart, layout.openBookmarksButtonEnd, 1, Height - 4
    ));
    findButton_->setViewport(viewport.subRect(
        layout.findButtonStart, layout.findButtonEnd, 1, Height - 4
    ));
    if(qualitySelector_) {
        qualitySelector_->setViewport(viewport.subRect(
            layout.qualitySelectorStart,
            layout.qualitySelectorEnd,
            1,
            Height - 4
        ));
    }
    if(clipboardButtonEnabled_) {
        clipboardButton_->setViewport(viewport.subRect(
            layout.clipboardButtonStart, layout.clipboardButtonEnd, 1, Height - 4
        ));
    }
    if(isDownloadVisible_()) {
        downloadButton_->setViewport(viewport.subRect(
            layout.downloadStart, layout.downloadEnd,
            1, Height - 4 - (downloadProgress_.empty() ? 0 : 5)
        ));
    }
    if(findBarVisible_) {
        findBar_->setViewport(viewport.subRect(
            layout.findBarStart, layout.findBarEnd,
            1, Height - 4
        ));
    } else {
        findBar_->setViewport(ImageSlice());
    }
}

void ControlBar::widgetRender_() {
    REQUIRE_UI_THREAD();

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

    // Separators
    viewport.fill(layout.separator1Pos - 1, layout.separator1Pos, 1, Height - 4, 128);
    viewport.fill(layout.separator1Pos, layout.separator1Pos + 1, 1, Height - 4, 255);
    if(layout.separator2Visible) {
        viewport.fill(layout.separator2Pos - 1, layout.separator2Pos, 1, Height - 4, 128);
        viewport.fill(layout.separator2Pos, layout.separator2Pos + 1, 1, Height - 4, 255);
    }
    if(layout.separator3Visible) {
        viewport.fill(layout.separator3Pos - 1, layout.separator3Pos, 1, Height - 4, 128);
        viewport.fill(layout.separator3Pos, layout.separator3Pos + 1, 1, Height - 4, 255);
    }

    // "Quality" text
    if(qualitySelector_) {
        qualityText_->render(
            viewport.subRect(
                layout.qualityTextStart, layout.qualityTextEnd,
                1, Height - 4
            ),
            3, -4
        );
    }

    // "Find" text
    if(findBarVisible_) {
        findText_->render(
            viewport.subRect(layout.findTextStart, layout.findTextEnd, 1, Height - 4),
            3, -4
        );
    }

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
    REQUIRE_UI_THREAD();
    vector<shared_ptr<Widget>> children = {
        addrField_,
        goButton_,
        bookmarkToggleButton_,
        openBookmarksButton_,
        findButton_
    };
    if(qualitySelector_) {
        children.push_back(qualitySelector_);
    }
    if(clipboardButtonEnabled_) {
        children.push_back(clipboardButton_);
    }
    if(isDownloadVisible_()) {
        children.push_back(downloadButton_);
    }
    if(findBarVisible_) {
        children.push_back(findBar_);
    }
    return children;
}

}
