#pragma once

#include "widget.hpp"

class CefBrowser;
class CefRenderHandler;

class BrowserArea :
    public Widget,
    public enable_shared_from_this<BrowserArea>
{
SHARED_ONLY_CLASS(BrowserArea);
public:
    BrowserArea(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler);
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

    CefRefPtr<CefBrowser> browser_;
};
