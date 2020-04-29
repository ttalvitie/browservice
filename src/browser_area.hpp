#pragma once

#include "widget.hpp"

class BrowserAreaEventHandler {
public:
    virtual void onBrowserAreaViewDirty() = 0;
};

class CefBrowser;
class CefRenderHandler;

// BrowserArea is a special widget in the sense that it renders continuously
// (outside render() calls) and does not notify of updates using
// WidgetEventHandler::onWidgetViewDirty; instead, it calls
// BrowserAreaEventHandler::onBrowserAreaViewDirty. This is to avoid redrawing
// the rest of the UI every time the browser area updates.
class BrowserArea :
    public Widget,
    public enable_shared_from_this<BrowserArea>
{
SHARED_ONLY_CLASS(BrowserArea);
public:
    BrowserArea(CKey,
        weak_ptr<WidgetEventHandler> widgetEventHandler,
        weak_ptr<BrowserAreaEventHandler> eventHandler
    );
    ~BrowserArea();

    // Creates a new CefRenderHandler than retains a pointer to this BrowserArea
    // and paints the browser contents to the viewport
    CefRefPtr<CefRenderHandler> createCefRenderHandler();

    // Sets the browser that will be kept up to date about size changes of
    // this widget. The browser can be unset by passing a null pointer.
    void setBrowser(CefRefPtr<CefBrowser> browser);

private:
    class RenderHandler;

    virtual void widgetViewportUpdated_() override;

    weak_ptr<BrowserAreaEventHandler> eventHandler_;
    CefRefPtr<CefBrowser> browser_;
};
