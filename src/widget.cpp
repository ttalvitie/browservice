#include "widget.hpp"

#include "key.hpp"

Widget::Widget(weak_ptr<WidgetParent> parent) {
    CEF_REQUIRE_UI_THREAD();

    parent_ = parent;
    viewDirty_ = false;

    mouseOver_ = false;
    focused_ = false;

    lastMouseX_ = -1;
    lastMouseY_ = -1;

    cursor_ = NormalCursor;
    myCursor_ = NormalCursor;
}

void Widget::setViewport(ImageSlice viewport) {
    CEF_REQUIRE_UI_THREAD();

    viewport_ = viewport;
    widgetViewportUpdated_();
    signalViewDirty_();
}

ImageSlice Widget::getViewport() {
    CEF_REQUIRE_UI_THREAD();
    return viewport_;
}

void Widget::render() {
    CEF_REQUIRE_UI_THREAD();

    viewDirty_ = false;
    widgetRender_();

    for(shared_ptr<Widget> child : widgetListChildren_()) {
        CHECK(child);
        child->render();
    }
}

int Widget::cursor() {
    CEF_REQUIRE_UI_THREAD();
    return cursor_;
}

void Widget::sendMouseDownEvent(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    if(mouseButtonsDown_.count(button)) {
        return;
    }

    updateFocus_(x, y);

    mouseButtonsDown_.insert(button);
    forwardMouseDownEvent_(x, y, button);
}

void Widget::sendMouseUpEvent(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    if(!mouseButtonsDown_.count(button)) {
        return;
    }

    mouseButtonsDown_.erase(button);
    forwardMouseUpEvent_(x, y, button);

    updateMouseOver_(x, y);
}

void Widget::sendMouseDoubleClickEvent(int x, int y) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    forwardMouseDoubleClickEvent_(x, y);
}

void Widget::sendMouseWheelEvent(int x, int y, int delta) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    updateMouseOver_(x, y);
    forwardMouseWheelEvent_(x, y, delta);
}

void Widget::sendMouseMoveEvent(int x, int y) {
    CEF_REQUIRE_UI_THREAD();

    if(!mouseOver_ && x == lastMouseX_ && y == lastMouseY_) {
        return;
    }

    lastMouseX_ = x;
    lastMouseY_ = y;

    updateMouseOver_(x, y);
    forwardMouseMoveEvent_(x, y);
}

void Widget::sendMouseEnterEvent(int x, int y) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    updateMouseOver_(x, y);
}

void Widget::sendMouseLeaveEvent(int x, int y) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    if(mouseButtonsDown_.empty() && mouseOver_) {
        forwardMouseLeaveEvent_(x, y);
        mouseOverChild_.reset();
        mouseOver_ = false;
        updateCursor_();
    }
}

void Widget::sendKeyDownEvent(int key) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(isValidKey(key));

    keysDown_.insert(key);
    forwardKeyDownEvent_(key);
}

void Widget::sendKeyUpEvent(int key) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(isValidKey(key));

    if(!keysDown_.count(key)) {
        return;
    }
    keysDown_.erase(key);
    forwardKeyUpEvent_(key);
}

void Widget::sendGainFocusEvent(int x, int y) {
    CEF_REQUIRE_UI_THREAD();

    lastMouseX_ = x;
    lastMouseY_ = y;

    updateFocus_(x, y);
}

void Widget::sendLoseFocusEvent() {
    CEF_REQUIRE_UI_THREAD();

    if(focused_) {
        clearEventState_(lastMouseX_, lastMouseY_);
        forwardLoseFocusEvent_();
        focusChild_.reset();
        focused_ = false;
    }
}

void Widget::onWidgetViewDirty() {
    CEF_REQUIRE_UI_THREAD();
    signalViewDirty_();
}

void Widget::onWidgetCursorChanged() {
    CEF_REQUIRE_UI_THREAD();
    updateCursor_();
}

void Widget::signalViewDirty_() {
    CEF_REQUIRE_UI_THREAD();

    if(!viewDirty_) {
        viewDirty_ = true;
        if(shared_ptr<WidgetParent> parent = parent_.lock()) {
            parent->onWidgetViewDirty();
        }
    }
}

void Widget::setCursor_(int newCursor) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(newCursor >= 0 && newCursor < CursorTypeCount);

    myCursor_ = newCursor;
    updateCursor_();
}

void Widget::updateFocus_(int x, int y) {
    shared_ptr<Widget> newFocusChild = childByPoint_(x, y);
    if(newFocusChild != focusChild_ || !focused_) {
        clearEventState_(x, y);
        updateMouseOver_(x, y);
        if(focused_) {
            forwardLoseFocusEvent_();
        }
        focusChild_ = newFocusChild;
        focused_ = true;
        forwardGainFocusEvent_(x, y);
    } else {
        updateMouseOver_(x, y);
    }
}

void Widget::updateMouseOver_(int x, int y) {
    if(!mouseButtonsDown_.empty()) {
        return;
    }

    shared_ptr<Widget> newMouseOverChild = childByPoint_(x, y);
    if(newMouseOverChild != mouseOverChild_ || !mouseOver_) {
        if(mouseOver_) {
            forwardMouseLeaveEvent_(x, y);
        }
        mouseOverChild_ = newMouseOverChild;
        mouseOver_ = true;
        forwardMouseEnterEvent_(x, y);
        updateCursor_();
    }
}

void Widget::clearEventState_(int x, int y) {
    while(!mouseButtonsDown_.empty()) {
        int button = *mouseButtonsDown_.begin();
        mouseButtonsDown_.erase(mouseButtonsDown_.begin());
        forwardMouseUpEvent_(x, y, button);
    }

    while(!keysDown_.empty()) {
        int key = *keysDown_.begin();
        keysDown_.erase(keysDown_.begin());
        forwardKeyUpEvent_(key);
    }
}

shared_ptr<Widget> Widget::childByPoint_(int x, int y) {
    for(shared_ptr<Widget> child : widgetListChildren_()) {
        CHECK(child);
        if(child->viewport_.containsGlobalPoint(x, y)) {
            return child;
        }
    }
    return {};
}

void Widget::updateCursor_() {
    int newCursor;
    if(mouseOverChild_) {
        newCursor = mouseOverChild_->cursor();
    } else {
        newCursor = myCursor_;
    }
    if(newCursor != cursor_) {
        cursor_ = newCursor;
        if(shared_ptr<WidgetParent> parent = parent_.lock()) {
            parent->onWidgetCursorChanged();
        }
    }
}

#define DEFINE_EVENT_FORWARD(name, widgetPtr, args, call) \
    void Widget::forward ## name ## Event_ args { \
        if(widgetPtr) { \
            widgetPtr->send ## name ## Event call; \
        } else { \
            EVENT_PREPROCESSING \
            widget ## name ## Event_ call; \
        } \
    }

#define EVENT_PREPROCESSING \
    x -= viewport_.globalX(); \
    y -= viewport_.globalY();
DEFINE_EVENT_FORWARD(
    MouseDown, focusChild_, (int x, int y, int button), (x, y, button)
);
DEFINE_EVENT_FORWARD(
    MouseUp, focusChild_, (int x, int y, int button), (x, y, button)
);
DEFINE_EVENT_FORWARD(
    MouseDoubleClick, focusChild_, (int x, int y), (x, y)
);
DEFINE_EVENT_FORWARD(
    MouseWheel, mouseOverChild_, (int x, int y, int delta), (x, y, delta)
);
DEFINE_EVENT_FORWARD(
    MouseMove, mouseOverChild_, (int x, int y), (x, y)
);
DEFINE_EVENT_FORWARD(
    MouseEnter, mouseOverChild_, (int x, int y), (x, y)
);
DEFINE_EVENT_FORWARD(
    MouseLeave, mouseOverChild_, (int x, int y), (x, y)
);
DEFINE_EVENT_FORWARD(
    GainFocus, focusChild_, (int x, int y), (x, y)
);
#undef EVENT_PREPROCESSING

#define EVENT_PREPROCESSING
DEFINE_EVENT_FORWARD(
    KeyDown, focusChild_, (int key), (key)
);
DEFINE_EVENT_FORWARD(
    KeyUp, focusChild_, (int key), (key)
);
DEFINE_EVENT_FORWARD(
    LoseFocus, focusChild_, (), ()
);
#undef EVENT_PREPROCESSING
