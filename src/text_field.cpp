#include "text_field.hpp"

#include "globals.hpp"
#include "key.hpp"
#include "text.hpp"
#include "timeout.hpp"
#include "xwindow.hpp"

TextField::TextField(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<TextFieldEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    requireUIThread();

    eventHandler_ = eventHandler;

    textLayout_ = OverflowTextLayout::create();

    hasFocus_ = false;
    leftMouseButtonDown_ = false;
    shiftKeyDown_ = false;
    controlKeyDown_ = false;

    caretActive_ = false;
    caretBlinkTimeout_ = Timeout::create(500);

    setCursor_(TextCursor);
}

void TextField::setText(string text) {
    requireUIThread();

    unsetCaret_();
    textLayout_->setText(move(text));
    textLayout_->setOffset(0);

    signalViewDirty_();
}

string TextField::text() {
    requireUIThread();
    return textLayout_->text();
}

bool TextField::hasFocus() {
    requireUIThread();
    return hasFocus_;
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
    requireUIThread();

    caretBlinkTimeout_->clear(false);

    weak_ptr<TextField> selfWeak = shared_from_this();
    caretBlinkTimeout_->set([selfWeak]() {
        requireUIThread();

        if(shared_ptr<TextField> self = selfWeak.lock()) {
            if(self->caretActive_) {
                self->caretBlinkState_ = !self->caretBlinkState_;
                self->signalViewDirty_();
                self->scheduleBlinkCaret_();
            }
        }
    });
}

void TextField::typeText_(const char* textPtr, int textLength) {
    if(caretActive_) {
        int idx1 = min(caretStart_, caretEnd_);
        int idx2 = max(caretStart_, caretEnd_);
        unsetCaret_();

        string oldText = textLayout_->text();
        CHECK(idx1 >= 0 && idx2 <= (int)oldText.size());

        string newText;
        newText.append(oldText, 0, idx1);
        newText.append(textPtr, textLength);
        newText.append(oldText, idx2, (int)oldText.size() - idx2);

        textLayout_->setText(newText);

        int idx = idx1 + textLength;
        setCaret_(idx, idx);
    }
}

void TextField::typeCharacter_(int key) {
    if(caretActive_) {
        UTF8Char utf8Char = keyToUTF8(key);
        typeText_((const char*)utf8Char.data, utf8Char.length);
    }
}

void TextField::eraseRange_() {
    if(caretActive_) {
        int idx1 = min(caretStart_, caretEnd_);
        int idx2 = max(caretStart_, caretEnd_);
        unsetCaret_();

        string oldText = textLayout_->text();
        CHECK(idx1 >= 0 && idx2 <= (int)oldText.size());

        string newText =
            oldText.substr(0, idx1) +
            oldText.substr(idx2, (int)oldText.size() - idx2);
        textLayout_->setText(newText);

        setCaret_(idx1, idx1);
    }
}

void TextField::pasteFromClipboard_() {
    if(caretActive_) {
        weak_ptr<TextField> selfWeak = shared_from_this();
        globals->xWindow->pasteFromClipboard([selfWeak](string text) {
            requireUIThread();
            if(shared_ptr<TextField> self = selfWeak.lock()) {
                self->typeText_(text.data(), text.size());
            }
        });
    }
}

void TextField::copyToClipboard_() {
    if(caretActive_) {
        int idx1 = min(caretStart_, caretEnd_);
        int idx2 = max(caretStart_, caretEnd_);
        if(idx1 < idx2) {
            string text = textLayout_->text();
            CHECK(idx1 >= 0 && idx2 <= (int)text.size());
            globals->xWindow->copyToClipboard(text.substr(idx1, idx2 - idx1));
        }
    }
}

void TextField::widgetViewportUpdated_() {
    requireUIThread();
    textLayout_->setWidth(getViewport().width());
}

void TextField::widgetRender_() {
    requireUIThread();

    ImageSlice viewport = getViewport();

    viewport.fill(0, viewport.width(), 0, viewport.height(), 255);

    textLayout_->render(viewport);

    int caretStartY = viewport.height() - 14;
    int caretEndY = viewport.height();

    if(caretActive_) {
        int startX = textLayout_->indexToXCoord(caretStart_);
        int endX = textLayout_->indexToXCoord(caretEnd_);

        ImageSlice fillSlice;
        if(startX < endX) {
            fillSlice = viewport.subRect(startX, endX, caretStartY, caretEndY);
        } else if(startX > endX) {
            fillSlice = viewport.subRect(endX + 1, startX, caretStartY, caretEndY);
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
            viewport.fill(endX, endX + 1, caretStartY, caretEndY, 0);
        }
    }
}

void TextField::widgetMouseDownEvent_(int x, int y, int button) {
    requireUIThread();

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
    requireUIThread();

    if(button == 0) {
        leftMouseButtonDown_ = false;
    }
}

void TextField::widgetMouseDoubleClickEvent_(int x, int y) {
    requireUIThread();
    setCaret_(0, (int)textLayout_->text().size());
}

void TextField::widgetMouseWheelEvent_(int x, int y, int delta) {
    requireUIThread();

    postTask(
        eventHandler_,
        &TextFieldEventHandler::onTextFieldWheelEvent,
        delta
    );
}

void TextField::widgetMouseMoveEvent_(int x, int y) {
    requireUIThread();

    if(leftMouseButtonDown_ && caretActive_) {
        int idx = textLayout_->xCoordToIndex(x);
        setCaret_(caretStart_, idx);
    }
}

void TextField::widgetKeyDownEvent_(int key) {
    requireUIThread();
    CHECK(isValidKey(key));

    if(key == keys::Shift) {
        shiftKeyDown_ = true;
    }
    if(key == keys::Control) {
        controlKeyDown_ = true;
    }

    if(key == keys::Down || key == keys::Up) {
        postTask(
            eventHandler_,
            &TextFieldEventHandler::onTextFieldUDKeyDown,
            key == keys::Down
        );
    }

    if((key == keys::Left || key == keys::Right) && caretActive_) {
        int idx = textLayout_->visualMoveIdx(caretEnd_, key == keys::Right);
        setCaret_(shiftKeyDown_ ? caretStart_ : idx, idx);
    }

    if(key > 0) {
        if(controlKeyDown_ && (key == (int)'c' || key == (int)'C')) {
            copyToClipboard_();
        } else if(controlKeyDown_ && (key == (int)'x' || key == (int)'X')) {
            copyToClipboard_();
            eraseRange_();
        } else if(controlKeyDown_ && (key == (int)'v' || key == (int)'V')) {
            pasteFromClipboard_();
        } else if(controlKeyDown_ && (key == (int)'a' || key == (int)'A')) {
            setCaret_(0, (int)textLayout_->text().size());
        } else {
            typeCharacter_(key);
        }
    }
    if(key == keys::Space) {
        typeCharacter_((int)' ');
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

void TextField::widgetKeyUpEvent_(int key) {
    requireUIThread();
    CHECK(isValidKey(key));

    if(key == keys::Down || key == keys::Up) {
        postTask(
            eventHandler_,
            &TextFieldEventHandler::onTextFieldUDKeyUp,
            key == keys::Down
        );
    }

    if(key == keys::Shift) {
        shiftKeyDown_ = false;
    }
    if(key == keys::Control) {
        controlKeyDown_ = false;
    }
}

void TextField::widgetGainFocusEvent_(int x, int y) {
    requireUIThread();
    hasFocus_ = true;
}

void TextField::widgetLoseFocusEvent_() {
    requireUIThread();

    hasFocus_ = false;

    if(caretActive_) {
        unsetCaret_();
        postTask(
            eventHandler_,
            &TextFieldEventHandler::onTextFieldLostFocusAfterEdit
        );
    }
}
