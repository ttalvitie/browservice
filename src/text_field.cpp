#include "text_field.hpp"

#include "text.hpp"
#include "timeout.hpp"

TextField::TextField(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<TextFieldEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    CEF_REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;

    textLayout_ = OverflowTextLayout::create();

    leftMouseButtonDown_ = false;
    shiftKeyDown_ = false;

    caretActive_ = false;
    caretBlinkTimeout_ = Timeout::create(500);

    setCursor_(TextCursor);
}

void TextField::setText(string text) {
    CEF_REQUIRE_UI_THREAD();

    unsetCaret_();
    textLayout_->setText(move(text));
    textLayout_->setOffset(0);

    signalViewDirty_();
}

string TextField::text() {
    CEF_REQUIRE_UI_THREAD();
    return textLayout_->text();
}

void TextField::unsetCaret_() {
    if(caretActive_) {
        caretActive_ = false;
        caretBlinkTimeout_->clear(false);
        signalViewDirty_();
    }
}

void TextField::setCaret_(int start, int end) {
    CHECK(start >= 0 && start <= (int)textLayout_->text().size());
    CHECK(end >= 0 && end <= (int)textLayout_->text().size());

    if(
        !caretActive_ ||
        caretStart_ != start ||
        caretEnd_ != end
    ) {
        caretActive_ = true;
        caretStart_ = start;
        caretEnd_ = end;
        caretBlinkState_ = true;

        textLayout_->makeVisible(end);

        scheduleBlinkCaret_();

        signalViewDirty_();
    }
}

void TextField::scheduleBlinkCaret_() {
    CEF_REQUIRE_UI_THREAD();

    caretBlinkTimeout_->clear(false);

    weak_ptr<TextField> selfWeak = shared_from_this();
    caretBlinkTimeout_->set([selfWeak]() {
        CEF_REQUIRE_UI_THREAD();

        if(shared_ptr<TextField> self = selfWeak.lock()) {
            if(self->caretActive_) {
                self->caretBlinkState_ = !self->caretBlinkState_;
                self->signalViewDirty_();
                self->scheduleBlinkCaret_();
            }
        }
    });
}

void TextField::typeCharacter_(string character) {
    if(caretActive_) {
        int idx1 = min(caretStart_, caretEnd_);
        int idx2 = max(caretStart_, caretEnd_);
        unsetCaret_();

        string oldText = textLayout_->text();
        string newText =
            oldText.substr(0, idx1) +
            character +
            oldText.substr(idx2, (int)oldText.size() - idx2);
        textLayout_->setText(newText);

        int idx = idx1 + character.size();
        setCaret_(idx, idx);
    }
}

void TextField::eraseRange_() {
    if(caretActive_) {
        int idx1 = min(caretStart_, caretEnd_);
        int idx2 = max(caretStart_, caretEnd_);
        unsetCaret_();

        string oldText = textLayout_->text();
        string newText =
            oldText.substr(0, idx1) +
            oldText.substr(idx2, (int)oldText.size() - idx2);
        textLayout_->setText(newText);

        setCaret_(idx1, idx1);
    }
}

void TextField::widgetViewportUpdated_() {
    CEF_REQUIRE_UI_THREAD();
    textLayout_->setWidth(getViewport().width());
}

void TextField::widgetRender_() {
    CEF_REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();

    viewport.fill(0, viewport.width(), 0, viewport.height(), 255);

    textLayout_->render(viewport);

    if(caretActive_) {
        int startX = textLayout_->indexToXCoord(caretStart_);
        int endX = textLayout_->indexToXCoord(caretEnd_);

        ImageSlice fillSlice;
        if(startX < endX) {
            fillSlice = viewport.subRect(startX, endX, 1, 15);
        } else if(startX > endX) {
            fillSlice = viewport.subRect(endX + 1, startX, 1, 15);
        }

        for(int y = 0; y < fillSlice.height(); ++y) {
            uint8_t* ptr = fillSlice.getPixelPtr(0, y);
            for(int x = 0; x < fillSlice.width(); ++x) {
                if(*ptr >= 128) {
                    *(ptr + 0) = 128;
                    *(ptr + 1) = 0;
                    *(ptr + 2) = 0;
                } else {
                    *(ptr + 0) = 255;
                    *(ptr + 1) = 255;
                    *(ptr + 2) = 255;
                }
                ptr += 4;
            }
        }

        if(caretBlinkState_) {
            viewport.fill(endX, endX + 1, 1, 15, 0);
        }
    }
}

void TextField::widgetMouseDownEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    if(button == 0) {
        if(caretActive_) {
            leftMouseButtonDown_ = true;
            int idx = textLayout_->xCoordToIndex(x);
            setCaret_(idx, idx);
        } else {
            setCaret_(0, (int)textLayout_->text().size());
        }
    }
}

void TextField::widgetMouseUpEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    if(button == 0) {
        leftMouseButtonDown_ = false;
    }
}

void TextField::widgetMouseDoubleClickEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    setCaret_(0, (int)textLayout_->text().size());
}

void TextField::widgetMouseMoveEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();

    if(leftMouseButtonDown_ && caretActive_) {
        int idx = textLayout_->xCoordToIndex(x);
        setCaret_(caretStart_, idx);
    }
}

void TextField::widgetKeyDownEvent_(Key key) {
    CEF_REQUIRE_UI_THREAD();

    if(key == keys::Shift) {
        shiftKeyDown_ = true;
    }

    if((key == keys::Left || key == keys::Right) && caretActive_) {
        int idx = textLayout_->visualMoveIdx(caretEnd_, key == keys::Right);
        setCaret_(shiftKeyDown_ ? caretStart_ : idx, idx);
    }

    if(key.isCharacter()) {
        typeCharacter_(key.character());
    }
    if(key == keys::Space) {
        typeCharacter_(" ");
    }

    if((key == keys::Backspace || key == keys::Delete) && caretActive_) {
        if(caretStart_ == caretEnd_) {
            caretEnd_ = textLayout_->visualMoveIdx(
                caretEnd_, key == keys::Delete
            );
        }
        eraseRange_();
    }

    if(key == keys::Enter && caretActive_) {
        unsetCaret_();
        string text = textLayout_->text();
        postTask(
            eventHandler_, &TextFieldEventHandler::onTextFieldSubmitted, text
        );
    }
}

void TextField::widgetKeyUpEvent_(Key key) {
    CEF_REQUIRE_UI_THREAD();

    if(key == keys::Shift) {
        shiftKeyDown_ = false;
    }
}

void TextField::widgetLoseFocusEvent_() {
    CEF_REQUIRE_UI_THREAD();
    unsetCaret_();
}
