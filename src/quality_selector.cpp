#include "quality_selector.hpp"

#include "globals.hpp"
#include "timeout.hpp"

QualitySelector::QualitySelector(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<QualitySelectorEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    CEF_REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;

    longMouseRepeatTimeout_ = Timeout::create(500);
    shortMouseRepeatTimeout_ = Timeout::create(50);

    quality_ = globals->defaultQuality;

    hasFocus_ = false;
    upKeyPressed_ = false;
    downKeyPressed_ = false;
    upButtonPressed_ = false;
    downButtonPressed_ = false;
    
    // Initialization is completed in afterConstruct_
}

void QualitySelector::onTextFieldSubmitted(string text) {
    CEF_REQUIRE_UI_THREAD();
    setQuality_(move(text));
}

void QualitySelector::onTextFieldLostFocusAfterEdit() {
    CEF_REQUIRE_UI_THREAD();
    setQuality_(textField_->text());
}

void QualitySelector::onTextFieldUDKeyDown(bool down) {
    CEF_REQUIRE_UI_THREAD();
    widgetKeyDownEvent_(down ? keys::Down : keys::Up);
}

void QualitySelector::onTextFieldUDKeyUp(bool down) {
    CEF_REQUIRE_UI_THREAD();
    widgetKeyUpEvent_(down ? keys::Down : keys::Up);
}

void QualitySelector::onTextFieldWheelEvent(int delta) {
    CEF_REQUIRE_UI_THREAD();

    if(delta != 0 && (hasFocus_ || textField_->hasFocus())) {
        setQuality_(quality_ + (delta > 0 ? 1 : -1));
    }
}

void QualitySelector::afterConstruct_(shared_ptr<QualitySelector> self) {
    textField_ = TextField::create(self, self);
    updateTextField_();
}

void QualitySelector::setQuality_(string qualityStr) {
    if(qualityStr == "PNG") {
        setQuality_(globals->PNGQuality);
    } else {
        optional<int> quality = parseString<int>(qualityStr);
        if(quality) {
            setQuality_(*quality);
        } else {
            updateTextField_();
        }
    }
}

void QualitySelector::setQuality_(int quality) {
    quality = min(quality, globals->maxQuality);
    quality = max(quality, globals->minQuality);

    if(quality_ != quality) {
        quality_ = quality;
        postTask(
            eventHandler_,
            &QualitySelectorEventHandler::onQualityChanged,
            quality
        );
        signalViewDirty_();
    }

    updateTextField_();
}

void QualitySelector::updateTextField_() {
    CHECK(quality_ >= globals->minQuality && quality_ <= globals->maxQuality);

    if(quality_ == globals->PNGQuality) {
        textField_->setText("PNG");
    } else {
        textField_->setText(toString(quality_));
    }
}

void QualitySelector::mouseRepeat_(int direction, bool first) {
    CEF_REQUIRE_UI_THREAD();

    int quality = quality_ + direction;
    if(quality >= globals->minQuality && quality <= globals->maxQuality) {
        setQuality_(quality);
    }

    weak_ptr<QualitySelector> selfWeak = shared_from_this();
    (first ? longMouseRepeatTimeout_ : shortMouseRepeatTimeout_)->set(
        [direction, selfWeak]() {
            if(shared_ptr<QualitySelector> self = selfWeak.lock()) {
                self->mouseRepeat_(direction, false);
            }
        }
    );
}

void QualitySelector::widgetViewportUpdated_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();
    textField_->setViewport(viewport.subRect(4, Width - 19, 2, Height - 4));
}

void QualitySelector::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();

    // Frame
    viewport.fill(0, Width - 1, 0, 1, 128);
    viewport.fill(0, 1, 1, Height - 1, 128);
    viewport.fill(0, Width, Height - 1, Height, 255);
    viewport.fill(Width - 1, Width, 0, Height - 1, 255);
    viewport.fill(1, Width - 2, 1, 2, 0);
    viewport.fill(1, 2, 2, Height - 2, 0);
    viewport.fill(1, Width - 1, Height - 2, Height - 1, 192);
    viewport.fill(Width - 2, Width - 1, 1, Height - 2, 192);

    // Text field background
    viewport.fill(2, Width - 17, 2, Height - 2, 255);

    // Buttons
    auto drawButton = [&](int startY, bool up, bool pressed, bool enabled) {
        if(!enabled) {
            pressed = false;
        }

        int startX = Width - 17;
        int endX = Width - 2;
        int endY = startY + 9;
        
        viewport.fill(startX, endX - 1, startY, startY + 1, pressed ? 128 : 192);
        viewport.fill(startX, startX + 1, startY + 1, endY - 1, pressed ? 128 : 192);
        viewport.fill(startX, endX, endY - 1, endY, pressed ? 255 : 0);
        viewport.fill(endX - 1, endX, startY, endY - 1, pressed ? 255 : 0);
        viewport.fill(startX + 1, endX - 2, startY + 1, startY + 2, pressed ? 0 : 255);
        viewport.fill(startX + 1, startX + 2, startY + 2, endY - 2, pressed ? 0 : 255);
        viewport.fill(startX + 1, endX - 1, endY - 2, endY - 1, pressed ? 192 : 128);
        viewport.fill(endX - 2, endX - 1, startY + 1, endY - 2, pressed ? 192 : 128);

        viewport.fill(startX + 2, endX - 2, startY + 2, endY - 2, 192);

        int arrowX = (startX + endX) / 2 + (int)pressed;
        int arrowY = startY + 4 + (int)pressed;
        int dy = up ? -1 : 1;
        if(!enabled) {
            viewport.fill(arrowX + 1, arrowX + 2, arrowY + 1 + dy, arrowY + 2 + dy, 255);
            viewport.fill(arrowX, arrowX + 3, arrowY + 1, arrowY + 2, 255);
            viewport.fill(arrowX - 1, arrowX + 4, arrowY + 1 - dy, arrowY + 2 - dy, 255);
        }
        viewport.fill(arrowX, arrowX + 1, arrowY + dy, arrowY + 1 + dy, enabled ? 0 : 128);
        viewport.fill(arrowX - 1, arrowX + 2, arrowY, arrowY + 1, enabled ? 0 : 128);
        viewport.fill(arrowX - 2, arrowX + 3, arrowY - dy, arrowY + 1 - dy, enabled ? 0 : 128);
    };
    drawButton(2, true, upKeyPressed_ || upButtonPressed_, quality_ < globals->maxQuality);
    drawButton(11, false, downKeyPressed_ || downButtonPressed_, quality_ > globals->minQuality);
}

vector<shared_ptr<Widget>> QualitySelector::widgetListChildren_() {
    CEF_REQUIRE_UI_THREAD();
    return {textField_};
}

void QualitySelector::widgetMouseDownEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    if(button == 0 && x >= Width - 17 && x <= Width - 2 && y >= 2 && y < 20) {
        longMouseRepeatTimeout_->clear(false);
        shortMouseRepeatTimeout_->clear(false);
        downButtonPressed_ = false;
        upButtonPressed_ = false;

        int direction;
        if(y < 11) {
            upButtonPressed_ = true;
            direction = 1;
        } else {
            downButtonPressed_ = true;
            direction = -1;
        }
        mouseRepeat_(direction, true);
        signalViewDirty_();
    }
}

void QualitySelector::widgetMouseUpEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    if(button == 0) {
        longMouseRepeatTimeout_->clear(false);
        shortMouseRepeatTimeout_->clear(false);
        downButtonPressed_ = false;
        upButtonPressed_ = false;
        signalViewDirty_();
    }
}

void QualitySelector::widgetMouseWheelEvent_(int x, int y, int delta) {
    CEF_REQUIRE_UI_THREAD();

    if(delta != 0 && (hasFocus_ || textField_->hasFocus())) {
        setQuality_(quality_ + (delta > 0 ? 1 : -1));
    }
}

void QualitySelector::widgetKeyDownEvent_(Key key) {
    CEF_REQUIRE_UI_THREAD();

    if(key == keys::Down || key == keys::Up) {
        downKeyPressed_ = key == keys::Down;
        upKeyPressed_ = key == keys::Up;

        int quality = quality_ + (key == keys::Down ? -1 : 1);
        if(quality >= globals->minQuality && quality <= globals->maxQuality) {
            setQuality_(quality);
            signalViewDirty_();
        }
    }
}

void QualitySelector::widgetKeyUpEvent_(Key key) {
    CEF_REQUIRE_UI_THREAD();

    if(key == keys::Down || key == keys::Up) {
        downKeyPressed_ = false;
        upKeyPressed_ = false;
        signalViewDirty_();
    }
}

void QualitySelector::widgetGainFocusEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    hasFocus_ = true;
}

void QualitySelector::widgetLoseFocusEvent_() {
    CEF_REQUIRE_UI_THREAD();
    hasFocus_ = false;
}
