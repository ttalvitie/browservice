#include "browser_area.hpp"

#include "include/cef_render_handler.h"

class BrowserArea::RenderHandler : public CefRenderHandler {
public:
    RenderHandler(shared_ptr<BrowserArea> browserArea)
        : browserArea_(browserArea)
    {}

    // CefRenderHandler:
    virtual void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
        CEF_REQUIRE_UI_THREAD();

        ImageSlice viewport = browserArea_->getViewport();
        int width = max(min(viewport.width(), 4096), 64);
        int height = max(min(viewport.height(), 4096), 64);

        rect.Set(0, 0, width, height);
    }
    virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& info) override {
        CEF_REQUIRE_UI_THREAD();

        CefRect rect;
        GetViewRect(browser, rect);

        info.device_scale_factor = 1.0;
        info.rect = rect;
        info.available_rect = rect;

        return true;
    }
    virtual void OnPaint(
        CefRefPtr<CefBrowser>,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int bufWidth,
        int bufHeight
    ) override {
        CEF_REQUIRE_UI_THREAD();

        if(type == PET_VIEW) {
            ImageSlice viewport = browserArea_->getViewport();
            bool updated = false;

            auto copyRange = [&](int y, int ax, int bx) {
                if(ax >= bx) {
                    return;
                }

                uint8_t* src = &((uint8_t*)buffer)[4 * (y * bufWidth + ax)];
                uint8_t* dest = viewport.getPixelPtr(ax, y);
                int byteCount = 4 * (bx - ax);

                if(updated) {
                    memcpy(dest, src, byteCount);
                } else {
                    if(memcmp(src, dest, byteCount)) {
                        updated = true;
                        memcpy(dest, src, byteCount);
                    }
                }
            };

            for(const CefRect& dirtyRect : dirtyRects) {
                int ax = max(dirtyRect.x, 0);
                int bx = min(
                    dirtyRect.x + dirtyRect.width,
                    min(viewport.width(), bufWidth)
                );
                int ay = max(dirtyRect.y, 0);
                int by = min(
                    dirtyRect.y + dirtyRect.height,
                    min(viewport.height(), bufHeight)
                );
                if(ax < bx) {
                    for(int y = ay; y < by; ++y) {
                        copyRange(y, ax, bx);
                    }
                }
            }

            if(updated) {
                postTask(
                    browserArea_->eventHandler_,
                    &BrowserAreaEventHandler::onBrowserAreaViewDirty
                );
            }
        }
    }

private:
    shared_ptr<BrowserArea> browserArea_;

    IMPLEMENT_REFCOUNTING(RenderHandler);
};

BrowserArea::BrowserArea(CKey,
    weak_ptr<WidgetEventHandler> widgetEventHandler,
    weak_ptr<BrowserAreaEventHandler> eventHandler
)
    : Widget(widgetEventHandler)
{
    CEF_REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    eventModifiers_ = 0;
}

BrowserArea::~BrowserArea() {}

CefRefPtr<CefRenderHandler> BrowserArea::createCefRenderHandler() {
    return new RenderHandler(shared_from_this());
}

void BrowserArea::setBrowser(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    browser_ = browser;

    if(browser_) {
        browser_->GetHost()->WasResized();
    }
}

void BrowserArea::widgetViewportUpdated_() {
    CEF_REQUIRE_UI_THREAD();

    if(browser_) {
        browser_->GetHost()->WasResized();
        browser_->GetHost()->Invalidate(PET_VIEW);
        browser_->GetHost()->Invalidate(PET_POPUP);
    }
}

namespace {

CefMouseEvent createMouseEvent(int x, int y, uint32_t eventModifiers) {
    CefMouseEvent event;
    event.x = x;
    event.y = y;
    event.modifiers = eventModifiers;
    return event;
}

pair<CefBrowserHost::MouseButtonType, uint32_t> getMouseButtonInfo(int button) {
    CefBrowserHost::MouseButtonType buttonType;
    uint32_t buttonFlag;
    if(button == 0) {
        buttonType = MBT_LEFT;
        buttonFlag = EVENTFLAG_LEFT_MOUSE_BUTTON;
    } else if(button == 1) {
        buttonType = MBT_MIDDLE;
        buttonFlag = EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    } else {
        buttonType = MBT_RIGHT;
        buttonFlag = EVENTFLAG_RIGHT_MOUSE_BUTTON;
    }
    return {buttonType, buttonFlag};
}

}

void BrowserArea::widgetMouseDownEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);

    CefBrowserHost::MouseButtonType buttonType;
    uint32_t buttonFlag;
    tie(buttonType, buttonFlag) = getMouseButtonInfo(button);

    browser_->GetHost()->SendMouseClickEvent(event, buttonType, false, 1);

    eventModifiers_ |= buttonFlag;
}

void BrowserArea::widgetMouseUpEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);

    CefBrowserHost::MouseButtonType buttonType;
    uint32_t buttonFlag;
    tie(buttonType, buttonFlag) = getMouseButtonInfo(button);

    browser_->GetHost()->SendMouseClickEvent(event, buttonType, true, 1);

    eventModifiers_ &= ~buttonFlag;
}

void BrowserArea::widgetMouseDoubleClickEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseClickEvent(event, MBT_LEFT, false, 2);
}

void BrowserArea::widgetMouseWheelEvent_(int x, int y, int delta) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseWheelEvent(event, 0, delta);
}

void BrowserArea::widgetMouseMoveEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, false);
}

void BrowserArea::widgetMouseEnterEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, false);
}

void BrowserArea::widgetMouseLeaveEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, true);
}

void BrowserArea::widgetKeyDownEvent_(Key key) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    LOG(INFO) << "Key events not implemented in BrowserArea";
}

void BrowserArea::widgetKeyUpEvent_(Key key) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    LOG(INFO) << "Key events not implemented in BrowserArea";
}

void BrowserArea::widgetGainFocusEvent_(int x, int y) {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->SendFocusEvent(true);
}

void BrowserArea::widgetLoseFocusEvent_() {
    CEF_REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->SendFocusEvent(false);
}
