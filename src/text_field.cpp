#include "text_field.hpp"

#include "clipboard.hpp"
#include "globals.hpp"
#include "key.hpp"
#include "text.hpp"
#include "timeout.hpp"

namespace browservice {

TextField::TextField(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<TextFieldEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;

    textLayout_ = OverflowTextLayout::create();

    removeCaretOnSubmit_ = true;
    allowEmptySubmit_ = true;

    hasFocus_ = false;
    leftMouseButtonDown_ = false;
    shiftKeyDown_ = false;
    controlKeyDown_ = false;

    caretActive_ = false;
    caretBlinkTimeout_ = Timeout::create(500);

    setCursor_(TextCursor);
}

void TextField::setText(string text) {
    REQUIRE_UI_THREAD();

    unsetCaret_();
    textLayout_->setText(move(text));
    textLayout_->setOffset(0);

    signalViewDirty_();
}

string TextField::text() {
    REQUIRE_UI_THREAD();
    return textLayout_->text();
}

void TextField::activate() {
    REQUIRE_UI_THREAD();

    takeFocus();
    setCaret_(0, (int)textLayout_->text().size());
}

bool TextField::hasFocus() {
    REQUIRE_UI_THREAD();
    return hasFocus_;
}

void TextField::setRemoveCaretOnSubmit(bool value) {
    REQUIRE_UI_THREAD();
    removeCaretOnSubmit_ = value;
}

void TextField::setAllowEmptySubmit(bool value) {
    REQUIRE_UI_THREAD();
    allowEmptySubmit_ = value;
}

void TextField::unsetCaret_() {
    if(caretActive_) {
        caretActive_ = false;
        caretBlinkTimeout_->clear(false);
        signalViewDirty_();
    }
}

void TextField::setCaret_(int start, int end) {
    REQUIRE(start >= 0 && start <= (int)textLayout_->text().size());
    REQUIRE(end >= 0 && end <= (int)textLayout_->text().size());

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
    REQUIRE_UI_THREAD();

    caretBlinkTimeout_->clear(false);

    weak_ptr<TextField> selfWeak = shared_from_this();
    caretBlinkTimeout_->set([selfWeak]() {
        REQUIRE_UI_THREAD();

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
        REQUIRE(idx1 >= 0 && idx2 <= (int)oldText.size());

        string newText;
        newText.append(oldText, 0, idx1);
        newText.append(textPtr, textLength);
        newText.append(oldText, idx2, (int)oldText.size() - idx2);

        textLayout_->setText(newText);

        int idx = idx1 + textLength;
        setCaret_(idx, idx);

        postTask(eventHandler_, &TextFieldEventHandler::onTextFieldTextChanged);
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
        REQUIRE(idx1 >= 0 && idx2 <= (int)oldText.size());

        string newText =
            oldText.substr(0, idx1) +
            oldText.substr(idx2, (int)oldText.size() - idx2);
        textLayout_->setText(newText);

        setCaret_(idx1, idx1);

        postTask(eventHandler_, &TextFieldEventHandler::onTextFieldTextChanged);
    }
}

void TextField::pasteFromClipboard_() {
    if(caretActive_) {
        weak_ptr<TextField> selfWeak = shared_from_this();
        string text = pasteFromClipboard();
        postTask([selfWeak, text]() {
            REQUIRE_UI_THREAD();
            if(shared_ptr<TextField> self = selfWeak.lock()) {
                self->typeText_(text.data(), (int)text.size());
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
            REQUIRE(idx1 >= 0 && idx2 <= (int)text.size());
            string slice = text.substr(idx1, idx2 - idx1);
            copyToClipboard(slice);
        }
    }
}

void TextField::widgetViewportUpdated_() {
    REQUIRE_UI_THREAD();
    textLayout_->setWidth(getViewport().width());
}

void TextField::widgetRender_() {
    REQUIRE_UI_THREAD();

    ImageSlice viewport = getViewport();

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
                uint8_t& r = *(ptr + 0);
                uint8_t& g = *(ptr + 1);
                uint8_t& b = *(ptr + 2);
                if(r == 0 && g == 0 && b == 0) {
                    r = 255;
                    g = 255;
                    b = 255;
                } else {
                    r = 128;
                    g = 0;
                    b = 0;
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
    REQUIRE_UI_THREAD();

    if(button == 0) {
        if(caretActive_) {
            leftMouseButtonDown_ = true;
            int idx = textLayout_->xCoordToIndex(x);
            if(shiftKeyDown_) {
                setCaret_(caretStart_, idx);
            } else {
                setCaret_(idx, idx);
            }
        } else {
            setCaret_(0, (int)textLayout_->text().size());
        }
    }
}

void TextField::widgetMouseUpEvent_(int x, int y, int button) {
    REQUIRE_UI_THREAD();

    if(button == 0) {
        leftMouseButtonDown_ = false;
    }
}

void TextField::widgetMouseDoubleClickEvent_(int x, int y) {
    REQUIRE_UI_THREAD();
    setCaret_(0, (int)textLayout_->text().size());
}

void TextField::widgetMouseWheelEvent_(int x, int y, int delta) {
    REQUIRE_UI_THREAD();

    postTask(
        eventHandler_,
        &TextFieldEventHandler::onTextFieldWheelEvent,
        delta
    );
}

void TextField::widgetMouseMoveEvent_(int x, int y) {
    REQUIRE_UI_THREAD();

    if(leftMouseButtonDown_ && caretActive_) {
        int idx = textLayout_->xCoordToIndex(x);
        setCaret_(caretStart_, idx);
    }
}

void TextField::widgetKeyDownEvent_(int key) {
    REQUIRE_UI_THREAD();
    REQUIRE(isValidKey(key));

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

    if(key == keys::Esc) {
        postTask(eventHandler_, &TextFieldEventHandler::onTextFieldEscKeyDown);
    }

    if((key == keys::Left || key == keys::Right) && caretActive_) {
        int idx = textLayout_->visualMoveIdx(caretEnd_, key == keys::Right);
        setCaret_(shiftKeyDown_ ? caretStart_ : idx, idx);
    }

    if((key == keys::Home || key == keys::End) && caretActive_) {
        int idx = key == keys::Home ? 0 : (int)textLayout_->text().size();
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
        string text = textLayout_->text();
        if(!text.empty() || allowEmptySubmit_) {
            if(removeCaretOnSubmit_) {
                unsetCaret_();
            }
            postTask(
                eventHandler_, &TextFieldEventHandler::onTextFieldSubmitted, text
            );
        }
    }
}

void TextField::widgetKeyUpEvent_(int key) {
    REQUIRE_UI_THREAD();
    REQUIRE(isValidKey(key));

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
    REQUIRE_UI_THREAD();
    hasFocus_ = true;
}

void TextField::widgetLoseFocusEvent_() {
    REQUIRE_UI_THREAD();

    hasFocus_ = false;

    if(caretActive_) {
        unsetCaret_();
        postTask(
            eventHandler_,
            &TextFieldEventHandler::onTextFieldLostFocusAfterEdit
        );
    }
}

}
