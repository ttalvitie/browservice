#include "browser_area.hpp"

#include "key.hpp"

#include "include/cef_render_handler.h"

class BrowserArea::RenderHandler : public CefRenderHandler {
public:
    RenderHandler(shared_ptr<BrowserArea> browserArea) {
        browserArea_ = browserArea;
    }

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

    virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override {
        CEF_REQUIRE_UI_THREAD();

        browserArea_->popupOpen_ = show;

        if(show) {
            browserArea_->popupRect_ = Rect();
        } else {
            browser->GetHost()->Invalidate(PET_VIEW);
        }
    }

    virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override {
        CEF_REQUIRE_UI_THREAD();

        browserArea_->popupRect_ = Rect(
            rect.x,
            rect.x + rect.width,
            rect.y,
            rect.y + rect.height
        );

        if(browserArea_->popupOpen_) {
            browser->GetHost()->Invalidate(PET_VIEW);
            browser->GetHost()->Invalidate(PET_POPUP);
        }
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

        ImageSlice viewport = browserArea_->getViewport();

        int offsetX = 0;
        int offsetY = 0;

        Rect bounds(0, viewport.width(), 0, viewport.height());
        Rect cutout;

        if(type == PET_VIEW) {
            if(browserArea_->popupOpen_) {
                cutout = browserArea_->popupRect_;
            }
        } else if(type == PET_POPUP && browserArea_->popupOpen_) {
            offsetX = browserArea_->popupRect_.startX;
            offsetY = browserArea_->popupRect_.startY;

            bounds = Rect::intersection(bounds, browserArea_->popupRect_);
            bounds = Rect::translate(bounds, -offsetX, -offsetY);
        } else {
            return;
        }

        bool updated = false;
        auto copyRange = [&](int y, int ax, int bx) {
            if(ax >= bx) {
                return;
            }

            uint8_t* src = &((uint8_t*)buffer)[4 * (y * bufWidth + ax)];
            uint8_t* dest = viewport.getPixelPtr(ax + offsetX, y + offsetY);
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
            Rect rect = Rect(
                dirtyRect.x,
                dirtyRect.x + dirtyRect.width,
                dirtyRect.y,
                dirtyRect.y + dirtyRect.height
            );
            rect = Rect::intersection(rect, bounds);

            if(!rect.isEmpty()) {
                for(int y = rect.startY; y < rect.endY; ++y) {
                    if(y >= cutout.startY && y < cutout.endY) {
                        copyRange(y, rect.startX, min(rect.endX, cutout.startX));
                        copyRange(y, max(rect.startX, cutout.endX), rect.endX);
                    } else {
                        copyRange(y, rect.startX, rect.endX);
                    }
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

    virtual void OnCursorChange(
        CefRefPtr<CefBrowser> browser,
        CefCursorHandle cursorHandle,
        CefRenderHandler::CursorType type,
        const CefCursorInfo& customCursorInfo
    ) override {
        CEF_REQUIRE_UI_THREAD();

        int cursor = NormalCursor;
        if(type == CT_HAND) cursor = HandCursor;
        if(type == CT_IBEAM) cursor = TextCursor;

        browserArea_->setCursor_(cursor);
    }

private:
    shared_ptr<BrowserArea> browserArea_;

    IMPLEMENT_REFCOUNTING(RenderHandler);
};

BrowserArea::BrowserArea(CKey,
    weak_ptr<WidgetParent> widgetParent,
    weak_ptr<BrowserAreaEventHandler> eventHandler
)
    : Widget(widgetParent)
{
    CEF_REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    popupOpen_ = false;
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

CefKeyEvent createKeyEvent(int key, uint32_t eventModifiers) {
    CHECK(isValidKey(key));

    CefKeyEvent event;
    event.windows_key_code = 0;
    event.native_key_code = 0;
    event.modifiers = eventModifiers;
    event.is_system_key = (bool)(event.modifiers & EVENTFLAG_ALT_DOWN);
    event.unmodified_character = 0;

    if(key < 0) {
        event.windows_key_code = -key;

        if(key == keys::Enter) {
            event.unmodified_character = '\r';
        }
        if(key == keys::Space) {
            event.unmodified_character = ' ';
        }
    } else {
        event.unmodified_character = key;
    }

    event.character = event.unmodified_character;
    return event;
}

uint32_t getKeyModifierFlag(int key) {
    if(key == keys::Shift) return EVENTFLAG_SHIFT_DOWN;
    if(key == keys::Control) return EVENTFLAG_CONTROL_DOWN;
    if(key == keys::Alt) return EVENTFLAG_ALT_DOWN;
    return 0;
}

}

void BrowserArea::widgetMouseDownEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    CefBrowserHost::MouseButtonType buttonType;
    uint32_t buttonFlag;
    tie(buttonType, buttonFlag) = getMouseButtonInfo(button);

    if(browser_) {
        CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
        browser_->GetHost()->SendMouseClickEvent(event, buttonType, false, 1);
    }

    eventModifiers_ |= buttonFlag;
}

void BrowserArea::widgetMouseUpEvent_(int x, int y, int button) {
    CEF_REQUIRE_UI_THREAD();

    CefBrowserHost::MouseButtonType buttonType;
    uint32_t buttonFlag;
    tie(buttonType, buttonFlag) = getMouseButtonInfo(button);

    if(browser_) {
        CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
        browser_->GetHost()->SendMouseClickEvent(event, buttonType, true, 1);
    }

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

void BrowserArea::widgetKeyDownEvent_(int key) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(isValidKey(key));

    if(browser_) {
        CefKeyEvent event = createKeyEvent(key, eventModifiers_);
        event.type = KEYEVENT_RAWKEYDOWN;
        browser_->GetHost()->SendKeyEvent(event);
        event.type = KEYEVENT_CHAR;
        browser_->GetHost()->SendKeyEvent(event);
    }

    eventModifiers_ |= getKeyModifierFlag(key);
}

void BrowserArea::widgetKeyUpEvent_(int key) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(isValidKey(key));

    if(browser_) {
        CefKeyEvent event = createKeyEvent(key, eventModifiers_);
        event.type = KEYEVENT_KEYUP;
        browser_->GetHost()->SendKeyEvent(event);
    }

    eventModifiers_ &= ~getKeyModifierFlag(key);
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
