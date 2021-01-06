#include "window.hpp"

#include "data_url.hpp"
#include "event.hpp"
#include "globals.hpp"
#include "key.hpp"
#include "timeout.hpp"
#include "root_widget.hpp"

#include "include/cef_client.h"

namespace browservice {

class Window::Client :
    public CefClient,
    public CefLifeSpanHandler,
    public CefLoadHandler,
    public CefDisplayHandler,
    public CefRequestHandler,
    public CefFindHandler,
    public CefKeyboardHandler
{
public:
    Client(shared_ptr<Window> window) {
        window_ = window;
        renderHandler_ =
            window->rootWidget_->browserArea()->createCefRenderHandler();
        downloadHandler_ =
            window->downloadManager_->createCefDownloadHandler();
        lastFindID_ = -1;
        certificateErrorPageSignKey_ = generateDataURLSignKey();
    }

    // CefClient:
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return renderHandler_;
    }
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }
    virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override {
        return this;
    }
    virtual CefRefPtr<CefDownloadHandler> GetDownloadHandler() override {
        return downloadHandler_;
    }
    virtual CefRefPtr<CefFindHandler> GetFindHandler() override {
        return this;
    }
    virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override {
        return this;
    }

    // CefLifeSpanHandler:
    virtual bool OnBeforePopup(
        CefRefPtr<CefBrowser> evBrowser,
        CefRefPtr<CefFrame> frame,
        const CefString&,
        const CefString&,
        CefLifeSpanHandler::WindowOpenDisposition,
        bool,
        const CefPopupFeatures&,
        CefWindowInfo& windowInfo,
        CefRefPtr<CefClient>& client,
        CefBrowserSettings& browserSettings,
        CefRefPtr<CefDictionaryValue>&,
        bool*
    ) override {
        REQUIRE_UI_THREAD();

        shared_ptr<WindowEventHandler> eventHandler = window_->eventHandler_;

        INFO_LOG("Window ", window_->handle_, " opening popup, TODO: implement");
        return true;
    }

    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        REQUIRE_UI_THREAD();
        REQUIRE(window_->state_ == Pending || window_->state_ == Closing);

        INFO_LOG("CEF browser for window ", window_->handle_, " created");

        window_->browser_ = browser;

        if(window_->state_ == Closing) {
            // We got close() while Pending and closing was deferred
            REQUIRE(window_->browser_);
            window_->browser_->GetHost()->CloseBrowser(true);
        } else {
            window_->state_ = Open;
        }

        window_->rootWidget_->browserArea()->setBrowser(browser);
    }

    virtual void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        REQUIRE_UI_THREAD();

        if(window_->state_ == Open) {
            window_->eventHandler_->onWindowClosing(window_->handle_);
            window_->state_ = Closing;
        }
        REQUIRE(window_->state_ == Closing);

        window_->state_ = Closed;
        window_->browser_ = nullptr;
        window_->rootWidget_->browserArea()->setBrowser(nullptr);
        window_->securityStatusUpdateTimeout_->clear(false);

        INFO_LOG("Window ", window_->handle_, " closed");

        window_->eventHandler_->onWindowClosed(window_->handle_);
    }

    // CefLoadHandler:
    virtual void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transitionType
    ) override {
        REQUIRE_UI_THREAD();

        if(frame->IsMain()) {
            if(readSignedDataURL(frame->GetURL(), certificateErrorPageSignKey_)) {
                window_->rootWidget_->browserArea()->showError(
                    "Loading URL failed due to a certificate error"
                );
            } else {
                window_->rootWidget_->browserArea()->clearError();
            }

            // Make sure that the loaded page gets the correct idea about the
            // focus and mouse over status
            window_->rootWidget_->browserArea()->refreshStatusEvents();
        }
    }

    virtual void OnLoadingStateChange(
        CefRefPtr<CefBrowser> browser,
        bool isLoading,
        bool canGoBack,
        bool canGoForward
    ) override {
        REQUIRE_UI_THREAD();
        window_->rootWidget_->controlBar()->setLoading(isLoading);
        window_->updateSecurityStatus_();
    }

    virtual void OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString& errorText,
        const CefString& failedURL
    ) override {
        REQUIRE_UI_THREAD();

        if(!frame->IsMain()) {
            return;
        }

        if(
            errorCode == ERR_ABORTED &&
            lastCertificateErrorURL_ &&
            *lastCertificateErrorURL_ == string(failedURL)
        ) {
            frame->LoadURL(
                createSignedDataURL(failedURL, certificateErrorPageSignKey_)
            );
        } else if(errorCode != ERR_ABORTED) {
            string msg = "Loading URL failed due to error: " + string(errorText);
            window_->rootWidget_->browserArea()->showError(msg);
            window_->rootWidget_->controlBar()->setAddress(failedURL);
        }
    }

    // CefDisplayHandler:
    virtual void OnAddressChange(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& url
    ) override {
        REQUIRE_UI_THREAD();

        optional<string> errorURL =
            readSignedDataURL(url, certificateErrorPageSignKey_);
        if(errorURL) {
            window_->rootWidget_->controlBar()->setAddress(*errorURL);
        } else {
            window_->rootWidget_->controlBar()->setAddress(url);
        }
        window_->updateSecurityStatus_();
    }

    virtual bool OnCursorChange(
        CefRefPtr<CefBrowser> browser,
        CefCursorHandle cursorHandle,
        cef_cursor_type_t type,
        const CefCursorInfo& customCursorInfo
    ) override {
        REQUIRE_UI_THREAD();

        int cursor = NormalCursor;
        if(type == CT_HAND) cursor = HandCursor;
        if(type == CT_IBEAM) cursor = TextCursor;

        window_->rootWidget_->browserArea()->setCursor(cursor);
        return true;
    }

    // CefRequestHandler:
    virtual CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool isNavigation,
        bool isDownload,
        const CefString& requestInitiator,
        bool& disableDefaultHandling
    ) override {
        CEF_REQUIRE_IO_THREAD();

        postTask(window_, &Window::updateSecurityStatus_);
        return nullptr;
    }

    virtual bool OnCertificateError(
        CefRefPtr<CefBrowser> browser,
        cef_errorcode_t certError,
        const CefString& requestURL,
        CefRefPtr<CefSSLInfo> sslInfo,
        CefRefPtr<CefRequestCallback> callback
    ) override {
        REQUIRE_UI_THREAD();

        lastCertificateErrorURL_ = string(requestURL);
        return false;
    }

    // CefFindHandler:
    virtual void OnFindResult(
        CefRefPtr<CefBrowser> browser,
        int identifier,
        int count,
        const CefRect& selectionRect,
        int activeMatchOrdinal,
        bool finalUpdate
    ) override {
        REQUIRE_UI_THREAD();

        if(identifier >= lastFindID_) {
            window_->rootWidget_->controlBar()->setFindResult(count > 0);
            lastFindID_ = identifier;
        }
    }

    // CefKeyboardHandler:
    virtual bool OnPreKeyEvent(
        CefRefPtr<CefBrowser> browser,
        const CefKeyEvent& event,
        CefEventHandle osEvent,
        bool* isKeyboardShortcut
    ) override {
        if(
            event.windows_key_code == -keys::Backspace &&
            !event.focus_on_editable_field
        ) {
            window_->navigate_(
                (event.modifiers & EVENTFLAG_SHIFT_DOWN) ? 1 : -1
            );
            return true;
        }
        return false;
    }

private:
    shared_ptr<Window> window_;

    CefRefPtr<CefRenderHandler> renderHandler_;
    CefRefPtr<CefDownloadHandler> downloadHandler_;

    int lastFindID_;

    optional<string> lastCertificateErrorURL_;
    string certificateErrorPageSignKey_;

    IMPLEMENT_REFCOUNTING(Client);
};

shared_ptr<Window> Window::tryCreate(
    shared_ptr<WindowEventHandler> eventHandler,
    uint64_t handle,
    bool isPopup
) {
    REQUIRE_UI_THREAD();

    shared_ptr<Window> window = Window::create(CKey());

    window->eventHandler_ = eventHandler;
    window->handle_ = handle;
    window->isPopup_ = isPopup;

    INFO_LOG("Opening window ", window->handle_);

    window->prePrevVisited_ = false;
    window->preMainVisited_ = false;

    window->prevNextClicked_ = false;

    window->curMainIdx_ = 0;
    window->curImgIdx_ = 0;
    window->curEventIdx_ = 0;

    window->curDownloadIdx_ = 0;

    window->state_ = Pending;

    window->lastNavigateOperationTime_ = steady_clock::now();

    window->securityStatusUpdateTimeout_ = Timeout::create(1000);

    window->rootViewport_ = ImageSlice::createImage(800, 600);

    window->rootWidget_ = RootWidget::create(window, window, window, true);
    window->rootWidget_->setViewport(window->rootViewport_);

    window->downloadManager_ = DownloadManager::create(window);

    if(!window->isPopup_) {
        CefRefPtr<CefClient> client = new Client(window);

        CefWindowInfo windowInfo;
        windowInfo.SetAsWindowless(kNullWindowHandle);

        CefBrowserSettings browserSettings;
        browserSettings.background_color = (cef_color_t)-1;

        if(!CefBrowserHost::CreateBrowser(
            windowInfo,
            client,
            globals->config->startPage,
            browserSettings,
            nullptr,
            nullptr
        )) {
            WARNING_LOG(
                "Opening browser for window ", window->handle_, " failed, ",
                "aborting window creation"
            );
            window->state_ = Closed;
            return {};
        }
    }

    window->updateSecurityStatus_();

    return window;
}

Window::Window(CKey, CKey) {}

Window::~Window() {
    REQUIRE(state_ == Closed);

    for(auto elem : downloads_) {
        elem.second.second->clear(false);
    }
}

void Window::close() {
    REQUIRE_UI_THREAD();

    if(state_ == Open) {
        INFO_LOG("Closing window ", handle_, " requested");
        state_ = Closing;
        REQUIRE(browser_);
        browser_->GetHost()->CloseBrowser(true);
        securityStatusUpdateTimeout_->clear(false);
    } else if(state_ == Pending) {
        INFO_LOG(
            "Closing window ", handle_,
            " requested while browser is still opening, deferring browser close"
        );

        state_ = Closing;
        securityStatusUpdateTimeout_->clear(false);
    } else {
        REQUIRE(false);
    }
}

ImageSlice Window::getViewImage() {
    return rootViewport_;
}

void Window::onWidgetViewDirty() {
    REQUIRE_UI_THREAD();

    shared_ptr<Window> self = shared_from_this();
    postTask([self]() {
        self->rootWidget_->render();
        self->sendViewportToCompressor_();
    });
}

void Window::onWidgetCursorChanged() {
    REQUIRE_UI_THREAD();

    shared_ptr<Window> self = shared_from_this();
    postTask([self]() {
        int cursor = self->rootWidget_->cursor();
        REQUIRE(cursor >= 0 && cursor < CursorTypeCount);
        INFO_LOG("Cursor changed to ", cursor, ", TODO: implement");
    });
}

void Window::onGlobalHotkeyPressed(GlobalHotkey key) {
    REQUIRE_UI_THREAD();

    shared_ptr<Window> self = shared_from_this();
    postTask([self, key]() {
        if(key == GlobalHotkey::Address) {
            self->rootWidget_->controlBar()->activateAddress();
        }
        if(key == GlobalHotkey::Find) {
            self->rootWidget_->controlBar()->openFindBar();
        }
        if(key == GlobalHotkey::FindNext) {
            self->rootWidget_->controlBar()->findNext();
        }
        if(key == GlobalHotkey::Refresh) {
            self->navigate_(0);
        }
    });
}

void Window::onAddressSubmitted(string url) {
    REQUIRE_UI_THREAD();

    if(!browser_) return;

    if(!url.empty()) {
        CefRefPtr<CefFrame> frame = browser_->GetMainFrame();
        if(frame) {
            frame->LoadURL(url);
            rootWidget_->browserArea()->takeFocus();
        }
    }
}

void Window::onQualityChanged(int quality) {
    REQUIRE_UI_THREAD();
    INFO_LOG("Quality change to ", quality, ", TODO: implement");
}

void Window::onPendingDownloadAccepted() {
    REQUIRE_UI_THREAD();
    downloadManager_->acceptPendingDownload();
}

void Window::onFind(string text, bool forward, bool findNext) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->Find(0, text, forward, false, findNext);
}

void Window::onStopFind(bool clearSelection) {
    REQUIRE_UI_THREAD();
    if(!browser_) return;

    browser_->GetHost()->StopFinding(clearSelection);
}

void Window::onClipboardButtonPressed() {
    REQUIRE_UI_THREAD();

    INFO_LOG("Clipboard button pressed (TODO: implement)");
}

void Window::onBrowserAreaViewDirty() {
    REQUIRE_UI_THREAD();
    sendViewportToCompressor_();
}

void Window::onPendingDownloadCountChanged(int count) {
    REQUIRE_UI_THREAD();
    rootWidget_->controlBar()->setPendingDownloadCount(count);
}

void Window::onDownloadProgressChanged(vector<int> progress) {
    REQUIRE_UI_THREAD();
    rootWidget_->controlBar()->setDownloadProgress(move(progress));
}

void Window::onDownloadCompleted(shared_ptr<CompletedDownload> file) {
    REQUIRE_UI_THREAD();

    INFO_LOG("Download completed (TODO: implement)");
}

void Window::updateSecurityStatus_() {
    REQUIRE_UI_THREAD();

    SecurityStatus securityStatus = SecurityStatus::Insecure;
    if(browser_) {
        CefRefPtr<CefNavigationEntry> nav = browser_->GetHost()->GetVisibleNavigationEntry();
        if(nav) {
            CefRefPtr<CefSSLStatus> sslStatus = nav->GetSSLStatus();
            if(
                sslStatus &&
                sslStatus->IsSecureConnection() &&
                !(sslStatus->GetCertStatus() & ~(
                    // non-error statuses
                    CERT_STATUS_IS_EV |
                    CERT_STATUS_REV_CHECKING_ENABLED |
                    CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                    CERT_STATUS_CT_COMPLIANCE_FAILED
                ))
            ) {
                if(sslStatus->GetContentStatus() == SSL_CONTENT_NORMAL_CONTENT) {
                    securityStatus = SecurityStatus::Secure;
                } else {
                    securityStatus = SecurityStatus::Warning;
                }
            }
        }
    }

    rootWidget_->controlBar()->setSecurityStatus(securityStatus);

    if(state_ == Pending || state_ == Open) {
        // Force update security status every once in a while just to make sure we
        // don't miss updates for a long time
        securityStatusUpdateTimeout_->clear(false);
        shared_ptr<Window> self = shared_from_this();
        securityStatusUpdateTimeout_->set([self]() {
            self->updateSecurityStatus_();
        });
    }
}

void Window::updateRootViewportSize_(int width, int height) {
    REQUIRE_UI_THREAD();

    width = max(min(width, 4096), 64);
    height = max(min(height, 4096), 64);

    if(rootViewport_.width() != width || rootViewport_.height() != height) {
        rootViewport_ = ImageSlice::createImage(width, height);
        rootWidget_->setViewport(rootViewport_);
    }
}

void Window::sendViewportToCompressor_() {
    if(state_ == Pending || state_ == Open) {
        eventHandler_->onWindowViewImageChanged(handle_);
    }
}

void Window::handleEvents_(
    uint64_t startIdx,
    string::const_iterator begin,
    string::const_iterator end
) {
    uint64_t eventIdx = startIdx;
    if(eventIdx > curEventIdx_) {
        WARNING_LOG(eventIdx - curEventIdx_, " events skipped in window ", handle_);
        curEventIdx_ = eventIdx;
    }

    string::const_iterator eventEnd = begin;
    while(true) {
        string::const_iterator eventBegin = eventEnd;
        while(true) {
            if(eventEnd >= end) {
                return;
            }
            if(*eventEnd == '/') {
                ++eventEnd;
                break;
            }
            ++eventEnd;
        }

        if(eventIdx == curEventIdx_) {
            if(!processEvent(rootWidget_, eventBegin, eventEnd)) {
                WARNING_LOG(
                    "Could not parse event '", string(eventBegin, eventEnd),
                    "' in window ", handle_
                );
            }
            ++eventIdx;
            curEventIdx_ = eventIdx;
        } else {
            ++eventIdx;
        }
    }
}

void Window::navigate_(int direction) {
    REQUIRE(direction >= -1 && direction <= 1);

    // If two navigation operations are too close together, they probably are
    // double-reported
    if(duration_cast<milliseconds>(
        steady_clock::now() - lastNavigateOperationTime_
    ).count() <= 200) {
        return;
    }
    lastNavigateOperationTime_ = steady_clock::now();

    if(browser_) {
        if(direction == -1) {
            browser_->GoBack();
        }
        if(direction == 0) {
            browser_->Reload();
        }
        if(direction == 1) {
            browser_->GoForward();
        }
    }
}

}
