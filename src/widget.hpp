#pragma once

#include "image_slice.hpp"

class WidgetEventHandler {
public:
    virtual void onWidgetViewDirty() = 0;
};

class Widget {
public:
    Widget(weak_ptr<WidgetEventHandler> eventHandler);

    void setViewport(ImageSlice viewport);
    ImageSlice getViewport();

    void render();

protected:
    // The widget should call this when its view has updated and the changes
    // should be rendered
    void signalViewDirty_();

    // Functions to be implemented by the widget:

    // Called after viewport (available through getViewport()) has been updated.
    // Does not need to call signalViewDirty_, as it is automatically called.
    virtual void widgetViewportUpdated_() {};

    // Called when widget should immediately ensure that it has been rendered
    // to the viewport (available through getViewport(), changes notified
    // through widgetViewportUpdated_ prior to this call). The widget is also
    // allowed to render to the viewport outside this function.
    virtual void widgetRender_() {};

private:
    weak_ptr<WidgetEventHandler> eventHandler_;
    ImageSlice viewport_;
    bool viewDirty_;
};
