#include "window.hpp"

#include "data_url.hpp"
#include "globals.hpp"
#include "root_widget.hpp"
#include "timeout.hpp"

#include "include/cef_client.h"

namespace browservice {

class Window::Client :
    public CefClient,
    public CefLifeSpanHandler,
    public CefLoadHandler,
    public CefDisplayHandler,
    public CefRequestHandler/*,
    public CefFindHandler,
    public CefKeyboardHandler*/
{
public:
    Client(shared_ptr<Window> window) {
        REQUIRE(window);
        window_ = window;

        REQUIRE(window->rootWidget_);
        renderHandler_ =
            window->rootWidget_->browserArea()->createCefRenderHandler();

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

    // CefLifeSpanHandler:
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        REQUIRE_UI_THREAD();
        REQUIRE(browser);
        REQUIRE(window_->state_ == Open || window_->state_ == Closed);
        REQUIRE(!window_->browser_);

        INFO_LOG("CEF browser for window ", window_->handle_, " created");

        window_->browser_ = browser;
        window_->rootWidget_->browserArea()->setBrowser(browser);

        window_->updateSecurityStatus_();

        if(window_->state_ == Closed) {
            // Browser close deferred from close().
            postTask([browser] {
                browser->GetHost()->CloseBrowser(true);
            });
        }
    }

    virtual void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        REQUIRE_UI_THREAD();
        REQUIRE(window_->state_ == Open || window_->state_ == Closed);
        REQUIRE(window_->browser_);

        if(window_->state_ == Open) {
            // The window closed on its own (not triggered by close()).
            INFO_LOG(
                "Closing window ", window_->handle_, " because ",
                "the CEF browser is closing"
            );
            REQUIRE(window_->eventHandler_);
            window_->state_ = Closed;
            window_->watchdogTimeout_->clear(false);
            window_->eventHandler_->onWindowClose(window_->handle_);
        }

        REQUIRE(window_->state_ == Closed);
        REQUIRE(window_->eventHandler_);

        INFO_LOG(
            "Cleanup of CEF browser for window ", window_->handle_, " complete"
        );

        window_->state_ = CleanupComplete;
        window_->browser_ = nullptr;
        window_->rootWidget_->browserArea()->setBrowser(nullptr);
        window_->eventHandler_->onWindowCleanupComplete(window_->handle_);
        window_->eventHandler_.reset();
    }

    // CefLoadHandler:
    virtual void OnLoadStart(
        CefRefPtr<CefBrowser>,
        CefRefPtr<CefFrame> frame,
        TransitionType transitionType
    ) override {
        REQUIRE_UI_THREAD();

        if(window_->state_ == Open && frame->IsMain()) {
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
        CefRefPtr<CefBrowser>,
        bool isLoading,
        bool canGoBack,
        bool canGoForward
    ) override {
        REQUIRE_UI_THREAD();

        if(window_->state_ == Open) {
            window_->rootWidget_->controlBar()->setLoading(isLoading);
            window_->updateSecurityStatus_();
        }
    }

    virtual void OnLoadError(
        CefRefPtr<CefBrowser>,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString& errorText,
        const CefString& failedURL
    ) override {
        REQUIRE_UI_THREAD();

        if(window_->state_ != Open || !frame->IsMain()) {
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
        CefRefPtr<CefBrowser>,
        CefRefPtr<CefFrame> frame,
        const CefString& url
    ) override {
        REQUIRE_UI_THREAD();

        if(window_->state_ != Open) {
            return;
        }

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
        CefRefPtr<CefBrowser>,
        CefCursorHandle cursorHandle,
        cef_cursor_type_t type,
        const CefCursorInfo& customCursorInfo
    ) override {
        REQUIRE_UI_THREAD();

        if(window_->state_ != Open) {
            return true;
        }

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

private:
    shared_ptr<Window> window_;
    CefRefPtr<CefRenderHandler> renderHandler_;

    optional<string> lastCertificateErrorURL_;
    string certificateErrorPageSignKey_;

    IMPLEMENT_REFCOUNTING(Client);
};

shared_ptr<Window> Window::tryCreate(
    shared_ptr<WindowEventHandler> eventHandler,
    uint64_t handle
) {
    REQUIRE_UI_THREAD();
    REQUIRE(eventHandler);
    REQUIRE(handle);

    INFO_LOG("Creating window ", handle);

    shared_ptr<Window> window = Window::create(CKey());

    window->handle_ = handle;
    window->state_ = Open;
    window->eventHandler_ = eventHandler;

    window->rootViewport_ = ImageSlice::createImage(800, 600);
    window->rootWidget_ = RootWidget::create(window, window, window, true);
    window->rootWidget_->setViewport(window->rootViewport_);

    window->watchdogTimeout_ = Timeout::create(1000);

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
            "Opening CEF browser for window ", handle, " failed, ",
            "aborting window creation"
        );
        window->state_ = CleanupComplete;
        window->eventHandler_.reset();
        return {};
    }

    postTask(window, &Window::watchdog_);

    return window;
}

Window::Window(CKey, CKey) {}

Window::~Window() {
    REQUIRE(state_ == CleanupComplete);
}

void Window::close() {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == Open);

    INFO_LOG("Closing window ", handle_);
    state_ = Closed;
    watchdogTimeout_->clear(false);

    // If the browser has been created, we start closing it; otherwise, we defer
    // closing it to Client::OnAfterCreated.
    if(browser_) {
        CefRefPtr<CefBrowser> browser = browser_;
        postTask([browser] {
            browser->GetHost()->CloseBrowser(true);
        });
    }
}

void Window::resize(int width, int height) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == Open);

    width = max(min(width, 4096), 64);
    height = max(min(height, 4096), 64);

    if(rootViewport_.width() != width || rootViewport_.height() != height) {
        rootViewport_ = ImageSlice::createImage(width, height);
        rootWidget_->setViewport(rootViewport_);
    }
}

ImageSlice Window::getViewImage() {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == Open);

    return rootViewport_;
}

void Window::onWidgetViewDirty() {
    REQUIRE_UI_THREAD();

    if(state_ != Open) {
        return;
    }

    shared_ptr<Window> self = shared_from_this();
    postTask([self]() {
        if(self->state_ == Open) {
            self->rootWidget_->render();

            REQUIRE(self->eventHandler_);
            self->eventHandler_->onWindowViewImageChanged(self->handle_);
        }
    });
}

void Window::onBrowserAreaViewDirty() {
    REQUIRE_UI_THREAD();

    if(state_ == Open) {
        REQUIRE(eventHandler_);
        eventHandler_->onWindowViewImageChanged(handle_);
    }
}

// Called every 1s for an Open window for various checks.
void Window::watchdog_() {
    REQUIRE_UI_THREAD();

    if(state_ != Open) {
        return;
    }

    // Make sure that security status is not incorrect for extended periods of
    // time just in case our event handlers do not catch all the changes.
    updateSecurityStatus_();

    if(!watchdogTimeout_->isActive()) {
        weak_ptr<Window> selfWeak = shared_from_this();
        watchdogTimeout_->set([selfWeak]() {
            if(shared_ptr<Window> self = selfWeak.lock()) {
                self->watchdog_();
            }
        });
    }
}

void Window::updateSecurityStatus_() {
    REQUIRE_UI_THREAD();

    if(state_ != Open) {
        return;
    }

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
}

}
