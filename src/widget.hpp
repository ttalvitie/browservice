#pragma once

#include "image_slice.hpp"
#include "key.hpp"

static constexpr int HandCursor = 0;
static constexpr int NormalCursor = 1;
static constexpr int TextCursor = 2;
static constexpr int CursorTypeCount = 3;

class WidgetParent {
public:
    virtual void onWidgetViewDirty() = 0;
    virtual void onWidgetCursorChanged() = 0;
};

class Widget : public WidgetParent {
public:
    Widget(weak_ptr<WidgetParent> parent);

    void setViewport(ImageSlice viewport);
    ImageSlice getViewport();

    void render();

    int cursor();

    // Send input event to the widget or its descendants (focus handling
    // within the subtree is done automatically). The event is propagated to the
    // widget*Event_ handler function of the correct widget. The given mouse
    // coordinates should be global.
    void sendMouseDownEvent(int x, int y, int button);
    void sendMouseUpEvent(int x, int y, int button);
    void sendMouseDoubleClickEvent(int x, int y);
    void sendMouseWheelEvent(int x, int y, int delta);
    void sendMouseMoveEvent(int x, int y);
    void sendMouseEnterEvent(int x, int y);
    void sendMouseLeaveEvent(int x, int y);
    void sendKeyDownEvent(Key key);
    void sendKeyUpEvent(Key key);
    void sendGainFocusEvent(int x, int y);
    void sendLoseFocusEvent();

    // WidgetParent: (forward events from possible children)
    virtual void onWidgetViewDirty() override;
    virtual void onWidgetCursorChanged() override;

protected:
    // The widget should call this when its view has updated and the changes
    // should be rendered
    void signalViewDirty_();

    // The widget should call this to update its own cursor; the effects might
    // not be immediately visible if mouse is not over this widget
    void setCursor_(int newCursor);

    // Functions to be implemented by the widget:

    // Called after viewport (available through getViewport()) has been updated.
    // Does not need to call signalViewDirty_, as it is automatically called.
    virtual void widgetViewportUpdated_() {}

    // Called when widget should immediately ensure that it has been rendered
    // to the viewport (available through getViewport(), changes notified
    // through widgetViewportUpdated_ prior to this call). The widget is also
    // allowed to render to the viewport outside this function; however, it
    // is possible that some other widget (such as the parent) is drawing to
    // the same viewport.
    virtual void widgetRender_() {}

    // This function should list the child widgets of this widget; it is called
    // to route events to the correct widget.
    virtual vector<shared_ptr<Widget>> widgetListChildren_() {
        return {};
    }

    // Input event handlers for events targeted at this widget. Mouse
    // coordinates are local to the widget viewport.
    virtual void widgetMouseDownEvent_(int x, int y, int button) {}
    virtual void widgetMouseUpEvent_(int x, int y, int button) {}
    virtual void widgetMouseDoubleClickEvent_(int x, int y) {}
    virtual void widgetMouseWheelEvent_(int x, int y, int delta) {}
    virtual void widgetMouseMoveEvent_(int x, int y) {}
    virtual void widgetMouseEnterEvent_(int x, int y) {}
    virtual void widgetMouseLeaveEvent_(int x, int y) {}
    virtual void widgetKeyDownEvent_(Key key) {}
    virtual void widgetKeyUpEvent_(Key key) {}
    virtual void widgetGainFocusEvent_(int x, int y) {}
    virtual void widgetLoseFocusEvent_() {}

private:
    void updateFocus_(int x, int y);
    void updateMouseOver_(int x, int y);

    void clearEventState_(int x, int y);

    shared_ptr<Widget> childByPoint_(int x, int y);

    void updateCursor_();

    void forwardMouseDownEvent_(int x, int y, int button);
    void forwardMouseUpEvent_(int x, int y, int button);
    void forwardMouseDoubleClickEvent_(int x, int y);
    void forwardMouseWheelEvent_(int x, int y, int delta);
    void forwardMouseMoveEvent_(int x, int y);
    void forwardMouseEnterEvent_(int x, int y);
    void forwardMouseLeaveEvent_(int x, int y);
    void forwardKeyDownEvent_(Key key);
    void forwardKeyUpEvent_(Key key);
    void forwardGainFocusEvent_(int x, int y);
    void forwardLoseFocusEvent_();

    weak_ptr<WidgetParent> parent_;
    ImageSlice viewport_;
    bool viewDirty_;

    shared_ptr<Widget> focusChild_;
    shared_ptr<Widget> mouseOverChild_;

    bool mouseOver_;
    bool focused_;

    int lastMouseX_;
    int lastMouseY_;

    set<int> mouseButtonsDown_;
    set<Key> keysDown_;

    int cursor_;
    int myCursor_;
};
