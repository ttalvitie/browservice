#include "button.hpp"
#include "text.hpp"

Button::Button(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<ButtonEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    requireUIThread();

    eventHandler_ = eventHandler;
    enabled_ = false;
    textLayout_ = TextLayout::create();
}

void Button::setEnabled(bool enabled) {
    requireUIThread();

    if(enabled != enabled_) {
        enabled_ = enabled;
        mouseDown_ = false;
        pressed_ = false;
        signalViewDirty_();
    }
}

void Button::setText(string text) {
    requireUIThread();

    textLayout_->setText(move(text));
    signalViewDirty_();
}

void Button::widgetRender_() {
    requireUIThread();

    ImageSlice viewport = getViewport();

    int width = viewport.width();
    int height = viewport.height();
    bool pressed = enabled_ && pressed_;

    // Frame
    viewport.fill(0, width - 1, 0, 1, pressed ? 128 : 255);
    viewport.fill(0, 1, 1, height - 1, pressed ? 128 : 255);
    viewport.fill(0, width, height - 1, height, pressed ? 255 : 0);
    viewport.fill(width - 1, width, 0, height - 1, pressed ? 255 : 0);
    viewport.fill(1, width - 2, 1, 2, pressed ? 0 : 192);
    viewport.fill(1, 2, 2, height - 2, pressed ? 0 : 192);
    viewport.fill(1, width - 1, height - 2, height - 1, pressed ? 192 : 128);
    viewport.fill(width - 2, width - 1, 1, height - 2, pressed ? 192 : 128);

    // Background
    viewport.fill(2, width - 2, 2, height - 2, 192);

    // Text
    int offsetX = (width - textLayout_->width()) / 2;
    int offsetY = 7 - (height + 1) / 2;

    if(enabled_) {
        if(pressed_) {
            ++offsetX;
            ++offsetY;
        }
        textLayout_->render(viewport, offsetX, offsetY);
    } else {
        textLayout_->render(viewport, offsetX + 1, offsetY + 1, 255);
        textLayout_->render(viewport, offsetX, offsetY, 128);
    }
}

void Button::widgetMouseDownEvent_(int x, int y, int button) {
    requireUIThread();

    mouseDown_ = true;
    pressed_ = true;
    signalViewDirty_();
}

void Button::widgetMouseUpEvent_(int x, int y, int button) {
    requireUIThread();

    if(button == 0) {
        widgetMouseMoveEvent_(x, y);

        if(pressed_ && enabled_) {
            postTask(eventHandler_, &ButtonEventHandler::onButtonPressed);
        }

        mouseDown_ = false;
        pressed_ = false;
        signalViewDirty_();
    }
}

void Button::widgetMouseMoveEvent_(int x, int y) {
    requireUIThread();
    
    if(mouseDown_) {
        ImageSlice viewport = getViewport();
        bool inside =
            x >= 0 && y >= 0 &&
            x < viewport.width() && y < viewport.height();

        if(inside != pressed_) {
            pressed_ = inside;
            signalViewDirty_();
        }
    }
}
