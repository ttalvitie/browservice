#include "menu_button.hpp"

#include "key.hpp"

MenuButton::MenuButton(CKey,
    MenuButtonIcon icon,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<MenuButtonEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    requireUIThread();

    icon_ = icon;

    eventHandler_ = eventHandler;

    mouseOver_ = false;
    mouseDown_ = false;
}

void MenuButton::mouseMove_(int x, int y) {
    ImageSlice viewport = getViewport();

    bool newMouseOver =
        x >= 0 && x < viewport.width() &&
        y >= 0 && y < viewport.height();
    
    if(newMouseOver != mouseOver_) {
        mouseOver_ = newMouseOver;
        signalViewDirty_();
    }
}

void MenuButton::widgetRender_() {
    requireUIThread();

    ImageSlice viewport = getViewport();

    int width = icon_.first.width() + 3;
    int height = icon_.first.height() + 3;

    // Background
    viewport.fill(0, width, 0, height, 192);

    if(mouseOver_) {
        // Frame
        viewport.fill(0, width - 1, 0, 1, mouseDown_ ? 128 : 255);
        viewport.fill(0, 1, 1, height - 1, mouseDown_ ? 128 : 255);
        viewport.fill(0, width - 1, height - 1, height, mouseDown_ ? 255 : 128);
        viewport.fill(width - 1, width, 0, height, mouseDown_ ? 255 : 128);

        // Icon
        int d = mouseDown_ ? 1 : 0;
        viewport.putImage(icon_.first, 1 + d, 1 + d);
    } else {
        viewport.putImage(icon_.second, 1, 1);
    }
}

void MenuButton::widgetMouseDownEvent_(int x, int y, int button) {
    requireUIThread();

    if(button == 0) {
        mouseDown_ = true;
        signalViewDirty_();
    }
}

void MenuButton::widgetMouseUpEvent_(int x, int y, int button) {
    requireUIThread();

    if(button == 0) {
        if(mouseDown_ && mouseOver_) {
            weak_ptr<MenuButton> selfWeak = shared_from_this();
            postTask(
                eventHandler_,
                &MenuButtonEventHandler::onMenuButtonPressed,
                selfWeak
            );
        }

        mouseDown_ = false;
        signalViewDirty_();
    }
}

void MenuButton::widgetMouseMoveEvent_(int x, int y) {
    requireUIThread();
    mouseMove_(x, y);
}

void MenuButton::widgetMouseEnterEvent_(int x, int y) {
    requireUIThread();
    mouseMove_(x, y);
}

void MenuButton::widgetMouseLeaveEvent_(int x, int y) {
    requireUIThread();

    if(mouseOver_) {
        mouseOver_ = false;
        signalViewDirty_();
    }
}

void MenuButton::widgetKeyDownEvent_(int key) {
    requireUIThread();

    if(key == keys::Enter) {
        postTask(eventHandler_, &MenuButtonEventHandler::onMenuButtonEnterKeyDown);
    }
    if(key == keys::Esc) {
        postTask(eventHandler_, &MenuButtonEventHandler::onMenuButtonEscKeyDown);
    }
}
