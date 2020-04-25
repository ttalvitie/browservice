#include "browser_area.hpp"

#include "include/cef_render_handler.h"

namespace {

const int MinWidth = 64;
const int MinHeight = 64;

const int MaxWidth = 4096;
const int MaxHeight = 4096;

}

class BrowserArea::RenderHandler : public CefRenderHandler {
public:
    RenderHandler(shared_ptr<BrowserArea> browserArea)
        : browserArea_(browserArea)
    {}

    // CefRenderHandler:
    virtual void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
        CEF_REQUIRE_UI_THREAD();

        ImageSlice viewport = browserArea_->getViewport();
        int width = max(min(viewport.width(), MaxWidth), MinWidth);
        int height = max(min(viewport.height(), MaxHeight), MinHeight);

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
                browserArea_->signalViewDirty_();
            }
        }
    }

private:
    shared_ptr<BrowserArea> browserArea_;

    IMPLEMENT_REFCOUNTING(RenderHandler);
};

BrowserArea::BrowserArea(CKey, weak_ptr<WidgetEventHandler> widgetEventHandler)
    : Widget(widgetEventHandler)
{
    CEF_REQUIRE_UI_THREAD();
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
    }
}
