#include "browser_area.hpp"

#include "key.hpp"
#include "text.hpp"

#include "include/cef_render_handler.h"

namespace browservice {

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

#include "key_codes.hpp"

CefKeyEvent createKeyEvent(int key, uint32_t eventModifiers) {
    REQUIRE(isValidKey(key));

    CefKeyEvent event;
    event.windows_key_code = 0;
    event.native_key_code = 0;
    event.modifiers = eventModifiers;
    event.is_system_key = (bool)(event.modifiers & EVENTFLAG_ALT_DOWN);
    event.unmodified_character = 0;

    auto it = keyCodes.find(key);
    if(it != keyCodes.end()) {
        event.windows_key_code = it->second.first;
#ifdef __APPLE__
        event.native_key_code = it->second.second;
#ifndef _WIN32
#else
#endif
#else
#ifndef _WIN32
        event.native_key_code = it->second.second;
#endif
#endif
    }

    if(key < 0) {
        if(key == keys::Enter) {
            event.unmodified_character = '\r';
        }
        if(key == keys::Space) {
            event.unmodified_character = ' ';
        }
        if(key == keys::Backspace) {
#ifdef __APPLE__
            event.unmodified_character = '\b';
#else
            event.unmodified_character = '\b';
#endif
        }
    } else {
        event.unmodified_character = key;
    }
    event.character = event.unmodified_character;

    // Hack to avoid breaking AltGr keys
    if(
        key > 0 &&
        (event.modifiers & EVENTFLAG_CONTROL_DOWN) &&
        (event.modifiers & EVENTFLAG_ALT_DOWN) &&
        !(key >= (int)'a' && key <= (int)'z') &&
        !(key >= (int)'A' && key <= (int)'Z') &&
        !(key >= (int)'0' && key <= (int)'9')
    ) {
        event.modifiers &= ~EVENTFLAG_CONTROL_DOWN;
        event.modifiers &= ~EVENTFLAG_ALT_DOWN;
        event.is_system_key = false;
    }

    return event;
}

uint32_t getKeyModifierFlag(int key) {
    if(key == keys::Shift) return EVENTFLAG_SHIFT_DOWN;
    if(key == keys::Control) return EVENTFLAG_CONTROL_DOWN;
    if(key == keys::Alt) return EVENTFLAG_ALT_DOWN;
    return 0;
}

}

class BrowserArea::RenderHandler : public CefRenderHandler {
public:
    RenderHandler(shared_ptr<BrowserArea> browserArea) {
        browserArea_ = browserArea;
    }

    // CefRenderHandler:
    virtual void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
        REQUIRE_UI_THREAD();

        ImageSlice viewport = browserArea_->getViewport();
        int width = max(min(viewport.width(), 4096), 64);
        int height = max(min(viewport.height(), 4096), 64);

        rect.Set(0, 0, width, height);
    }

    virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& info) override {
        REQUIRE_UI_THREAD();

        CefRect rect;
        GetViewRect(browser, rect);

        info.device_scale_factor = 1.0;
        info.rect = rect;
        info.available_rect = rect;

        return true;
    }

    virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override {
        REQUIRE_UI_THREAD();

        browserArea_->popupOpen_ = show;

        if(show) {
            browserArea_->popupRect_ = Rect();
        } else {
            browser->GetHost()->Invalidate(PET_VIEW);
        }
    }

    virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override {
        REQUIRE_UI_THREAD();

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
        REQUIRE_UI_THREAD();

        ImageSlice viewport = browserArea_->getViewport();
        bool updated = false;

        if(browserArea_->errorActive_) {
            viewport.fill(0, viewport.width(), 0, viewport.height(), 255);
            browserArea_->errorLayout_->render(
                viewport.splitY(20).first, 7, 0, 96, 0, 0
            );
            updated = true;
        } else {
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
        }

        if(updated) {
            postTask(
                browserArea_->eventHandler_,
                &BrowserAreaEventHandler::onBrowserAreaViewDirty
            );
        }
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
    REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    popupOpen_ = false;
    eventModifiers_ = 0;
    errorActive_ = false;
    errorLayout_ = TextLayout::create();
}

BrowserArea::~BrowserArea() {}

CefRefPtr<CefRenderHandler> BrowserArea::createCefRenderHandler() {
    return new RenderHandler(shared_from_this());
}

void BrowserArea::setBrowser(CefRefPtr<CefBrowser> browser) {
    REQUIRE_UI_THREAD();

    browser_ = browser;

    if(browser_) {
        browser_->GetHost()->WasResized();
    }
}

void BrowserArea::refreshStatusEvents() {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->SetFocus(isFocused_());

    int x, y;
    tie(x, y) = getLastMousePos_();
    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, !isMouseOver_());
}

void BrowserArea::showError(string message) {
    REQUIRE_UI_THREAD();

    errorActive_ = true;
    errorLayout_->setText(message);

    if(browser_) {
        browser_->GetHost()->Invalidate(PET_VIEW);
        browser_->GetHost()->Invalidate(PET_POPUP);
    }
}

void BrowserArea::clearError() {
    REQUIRE_UI_THREAD();

    errorActive_ = false;
    errorLayout_->setText("");

    if(browser_) {
        browser_->GetHost()->Invalidate(PET_VIEW);
        browser_->GetHost()->Invalidate(PET_POPUP);
    }
}

void BrowserArea::setCursor(int cursor) {
    REQUIRE_UI_THREAD();
    setCursor_(cursor);
}

void BrowserArea::widgetViewportUpdated_() {
    REQUIRE_UI_THREAD();

    if(browser_) {
        browser_->GetHost()->WasResized();
        browser_->GetHost()->Invalidate(PET_VIEW);
        browser_->GetHost()->Invalidate(PET_POPUP);
    }
}

void BrowserArea::widgetMouseDownEvent_(int x, int y, int button) {
    REQUIRE_UI_THREAD();

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
    REQUIRE_UI_THREAD();

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
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseClickEvent(event, MBT_LEFT, false, 2);
    browser_->GetHost()->SendMouseClickEvent(event, MBT_LEFT, true, 2);
}

void BrowserArea::widgetMouseWheelEvent_(int x, int y, int delta) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseWheelEvent(event, 0, delta);
}

void BrowserArea::widgetMouseMoveEvent_(int x, int y) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, false);
}

void BrowserArea::widgetMouseEnterEvent_(int x, int y) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, false);
}

void BrowserArea::widgetMouseLeaveEvent_(int x, int y) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    CefMouseEvent event = createMouseEvent(x, y, eventModifiers_);
    browser_->GetHost()->SendMouseMoveEvent(event, true);
}

void BrowserArea::widgetKeyDownEvent_(int key) {
    REQUIRE_UI_THREAD();
    REQUIRE(isValidKey(key));

    if(browser_) {
        bool controlDown = (bool)(eventModifiers_ & EVENTFLAG_CONTROL_DOWN);
        bool xKey = key == (int)'x' || key == (int)'X';
        bool cKey = key == (int)'c' || key == (int)'C';
        bool vKey = key == (int)'v' || key == (int)'V';
        CefRefPtr<CefFrame> frame;
        if(
            controlDown &&
            (xKey || cKey || vKey) &&
            (frame = browser_->GetFocusedFrame())
        ) {
            if(xKey) frame->Cut();
            if(cKey) frame->Copy();
            if(vKey) frame->Paste();
        } else {
            CefKeyEvent event = createKeyEvent(key, eventModifiers_);

            // Debug logging
            printf("DEBUG: BrowserArea::widgetKeyDownEvent_ key=%d, char=0x%x, win=0x%x, nat=0x%x\n",
                key, event.character, event.windows_key_code, event.native_key_code);
            fflush(stdout);

            event.type = KEYEVENT_RAWKEYDOWN;
            browser_->GetHost()->SendKeyEvent(event);

#ifdef _WIN32
            if(event.character) {
                CefKeyEvent charEvent = event;
                charEvent.type = KEYEVENT_CHAR;
                charEvent.windows_key_code = charEvent.character;
                browser_->GetHost()->SendKeyEvent(charEvent);
            }
#else
            event.type = KEYEVENT_CHAR;
            browser_->GetHost()->SendKeyEvent(event);
#endif
        }
    }

    eventModifiers_ |= getKeyModifierFlag(key);
}

void BrowserArea::widgetKeyUpEvent_(int key) {
    REQUIRE_UI_THREAD();
    REQUIRE(isValidKey(key));

    if(browser_) {
        CefKeyEvent event = createKeyEvent(key, eventModifiers_);
        event.type = KEYEVENT_KEYUP;
        browser_->GetHost()->SendKeyEvent(event);
    }

    eventModifiers_ &= ~getKeyModifierFlag(key);
}

void BrowserArea::widgetGainFocusEvent_(int x, int y) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->SetFocus(true);
}

void BrowserArea::widgetLoseFocusEvent_() {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->SetFocus(false);
}

}
