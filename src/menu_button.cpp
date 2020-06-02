#include "menu_button.hpp"

namespace {

const vector<string> goIconPattern = {
    "GGGGGGGGGBGGGGG",
    "GGGGGGGGGBBGGGG",
    "GGGGGGGGGBUBGGG",
    "ccccccccccUuBGG",
    "cUUUUUUUUUUvvBG",
    "cUvMMMMMMMMMMdB",
    "cUMMMMMMMMMMddB",
    "cMDDDDDDDDMdDBG",
    "cbbbbbbbbbdDBGG",
    "GGGGGGGGGBDBGGG",
    "GGGGGGGGGBBGGGG",
    "GGGGGGGGGBGGGGG"
};

ImageSlice createActiveGoIcon() {
    return ImageSlice::createImageFromStrings(
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
    );
}

ImageSlice createPassiveGoIcon() {
    return ImageSlice::createImageFromStrings(
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
    );
}

}

const MenuButtonIcon GoIcon = {createActiveGoIcon(), createPassiveGoIcon()};
const MenuButtonIcon EmptyIcon;

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

    // Background
    viewport.fill(0, Width, 0, Height, 192);

    const int IconX = 4;
    const int IconY = 5;

    if(mouseOver_) {
        // Frame
        viewport.fill(0, Width - 1, 0, 1, mouseDown_ ? 128 : 255);
        viewport.fill(0, 1, 1, Height - 1, mouseDown_ ? 128 : 255);
        viewport.fill(0, Width - 1, Height - 1, Height, mouseDown_ ? 255 : 128);
        viewport.fill(Width - 1, Width, 0, Height, mouseDown_ ? 255 : 128);

        // Icon
        int d = mouseDown_ ? 1 : 0;
        viewport.putImage(icon_.first, IconX + d, IconY + d);
    } else {
        viewport.putImage(icon_.second, IconX, IconY);
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
    postTask(eventHandler_, &MenuButtonEventHandler::onMenuButtonEnterKeyDown);
}
