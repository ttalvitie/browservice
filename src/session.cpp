#include "session.hpp"

#include "event.hpp"
#include "globals.hpp"
#include "html.hpp"
#include "image_compressor.hpp"
#include "key.hpp"
#include "timeout.hpp"
#include "root_widget.hpp"

#include "include/cef_client.h"
#include "include/cef_parser.h"

namespace {

set<uint64_t> usedSessionIDs;
mt19937 sessionIDRNG(random_device{}());

regex mainPathRegex("/[0-9]+/");
regex prevPathRegex("/[0-9]+/prev/");
regex nextPathRegex("/[0-9]+/next/");
regex imagePathRegex(
    "/[0-9]+/image/([0-9]+)/([0-9]+)/([01])/([0-9]+)/([0-9]+)/([0-9]+)/(([A-Z0-9_-]+/)*)"
);
regex iframePathRegex(
    "/[0-9]+/iframe/([0-9]+)/[0-9]+/"
);
regex downloadPathRegex(
    "/[0-9]+/download/([0-9]+)/.*"
);
regex closePathRegex(
    "/[0-9]+/close/([0-9]+)/"
);

}

class Session::Client :
    public CefClient,
    public CefLifeSpanHandler,
    public CefLoadHandler,
    public CefDisplayHandler,
    public CefRequestHandler,
    public CefFindHandler
{
public:
    Client(shared_ptr<Session> session) {
        session_ = session;
        renderHandler_ =
            session->rootWidget_->browserArea()->createCefRenderHandler();
        downloadHandler_ =
            session->downloadManager_->createCefDownloadHandler();
        lastFindID_ = -1;
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
        requireUIThread();

        shared_ptr<SessionEventHandler> eventHandler =
            session_->eventHandler_.lock();
        if(!eventHandler) {
            return true;
        }

        LOG(INFO) << "Session " << session_->id() << " opening popup";

        if(eventHandler->onIsServerFullQuery()) {
            LOG(INFO) << "Aborting popup creation due to session limit";
            return true;
        }

        browserSettings.background_color = (cef_color_t)-1;
        windowInfo.SetAsWindowless(kNullWindowHandle);

        shared_ptr<Session> popupSession =
            Session::create(session_->eventHandler_, session_->allowPNG_, true);
        client = new Client(popupSession);

        eventHandler->onPopupSessionOpen(popupSession);

        uint64_t popupSessionID = popupSession->id();
        session_->addIframe_([popupSessionID](shared_ptr<HTTPRequest> request) {
            request->sendHTMLResponse(
                200,
                writePopupIframeHTML,
                {popupSessionID}
            );
        });

        return false;
    }

    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        requireUIThread();
        CHECK(session_->state_ == Pending);

        LOG(INFO) << "CEF browser for session " << session_->id_ << " created";

        session_->browser_ = browser;
        session_->state_ = Open;
        session_->rootWidget_->browserArea()->setBrowser(browser);

        if(session_->closeOnOpen_) {
            session_->close();
        }
    }

    virtual void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        requireUIThread();
        CHECK(session_->state_ == Open || session_->state_ == Closing);

        session_->state_ = Closed;
        session_->browser_ = nullptr;
        session_->rootWidget_->browserArea()->setBrowser(nullptr);
        session_->imageCompressor_->flush();

        LOG(INFO) << "Session " << session_->id_ << " closed";

        postTask(session_->eventHandler_, &SessionEventHandler::onSessionClosed, session_->id_);
        session_->updateInactivityTimeout_();
    }

    // CefLoadHandler:
    virtual void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transitionType
    ) override {
        requireUIThread();

        if(frame->IsMain()) {
            // Make sure that the loaded page gets the correct idea about the
            // focus and mouse over status
            session_->rootWidget_->browserArea()->refreshStatusEvents();
        }
    }

    virtual void OnLoadingStateChange(
        CefRefPtr<CefBrowser> browser,
        bool isLoading,
        bool canGoBack,
        bool canGoForward
    ) override {
        requireUIThread();
        session_->rootWidget_->controlBar()->setLoading(isLoading);
        session_->updateSecurityStatus_();
    }

    virtual void OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString& errorText,
        const CefString& failedURL
    ) override {
        requireUIThread();

        bool certError =
            errorCode == ERR_ABORTED &&
            lastCertificateErrorURL_ &&
            *lastCertificateErrorURL_ == string(failedURL) &&
            frame->IsMain();

        // Do not show error for file downloads
        if(!certError && errorCode == ERR_ABORTED) {
            return;
        }

        stringstream ss;
        ss << "<html><head><title>Load error</title></head>";
        ss << "<body><h2>";

        ss << "Loading URL " << string(failedURL) << " failed";
        if(certError) {
            ss << " due to a certificate error";
        } else {
            ss << " with error " << string(errorText);
        }

        // Add random data to avoid collisions with actual URLs in ignoredURL_
        mt19937 rng(random_device{}());
        ss << "<!--";
        for(int i = 0; i < 16; ++i) {
            ss << (char)uniform_int_distribution<int>('A', 'Z')(rng);
        }
        ss << "-->";

        ss << "</h2></body></html>";
        string errorHTML = ss.str();

        string dataURL = "data:text/html;base64,";
        dataURL += string(CefURIEncode(
            CefBase64Encode(errorHTML.data(), errorHTML.size()),
            false
        ));

        if(frame->IsMain()) {
            // Make the addres bar show the failed URL instead of the data URL
            ignoredURL_ = dataURL;
            session_->rootWidget_->controlBar()->setAddress(failedURL);
        }

        frame->LoadURL(dataURL);
    }

    // CefDisplayHandler:
    virtual void OnAddressChange(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& url
    ) override {
        requireUIThread();

        if(!ignoredURL_ || string(url) != *ignoredURL_) {
            session_->rootWidget_->controlBar()->setAddress(url);
        }
        session_->updateSecurityStatus_();
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

        postTask(session_, &Session::updateSecurityStatus_);
        return nullptr;
    }

    virtual bool OnCertificateError(
        CefRefPtr<CefBrowser> browser,
        cef_errorcode_t certError,
        const CefString& requestURL,
        CefRefPtr<CefSSLInfo> sslInfo,
        CefRefPtr<CefRequestCallback> callback
    ) override {
        requireUIThread();

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
        requireUIThread();

        if(identifier > lastFindID_) {
            session_->rootWidget_->controlBar()->setFindResult(count > 0);
            lastFindID_ = identifier;
        }
    }

private:
    shared_ptr<Session> session_;

    CefRefPtr<CefRenderHandler> renderHandler_;
    CefRefPtr<CefDownloadHandler> downloadHandler_;

    int lastFindID_;

    optional<string> ignoredURL_;
    optional<string> lastCertificateErrorURL_;

    IMPLEMENT_REFCOUNTING(Client);
};

Session::Session(CKey,
    weak_ptr<SessionEventHandler> eventHandler,
    bool allowPNG,
    bool isPopup
) {
    requireUIThread();

    eventHandler_ = eventHandler;

    isPopup_ = isPopup;

    while(true) {
        id_ = uniform_int_distribution<uint64_t>()(sessionIDRNG);
        if(!usedSessionIDs.count(id_)) {
            break;
        }
    }
    usedSessionIDs.insert(id_);

    LOG(INFO) << "Opening session " << id_;

    prePrevVisited_ = false;
    preMainVisited_ = false;

    prevNextClicked_ = false;

    curMainIdx_ = 0;
    curImgIdx_ = 0;
    curEventIdx_ = 0;

    curDownloadIdx_ = 0;

    state_ = Pending;

    closeOnOpen_ = false;

    inactivityTimeoutLong_ = Timeout::create(30000);
    inactivityTimeoutShort_ = Timeout::create(4000);

    allowPNG_ = allowPNG;

    lastSecurityStatusUpdateTime_ = steady_clock::now();

    imageCompressor_ = ImageCompressor::create(2000, allowPNG_);

    paddedRootViewport_ = ImageSlice::createImage(
        800 + WidthSignalModulus - 1,
        600 + HeightSignalModulus - 1
    );
    rootViewport_ = paddedRootViewport_.subRect(0, 800, 0, 600);

    widthSignal_ = WidthSignalNoNewIframe;
    heightSignal_ = NormalCursor;

    // Initialization is finalized in afterConstruct_
}

Session::~Session() {
    for(auto elem : downloads_) {
        elem.second.second->clear(false);
    }

    uint64_t id = id_;
    postTask([id]() {
        usedSessionIDs.erase(id);
    });
}

void Session::close() {
    requireUIThread();

    if(state_ == Open) {
        LOG(INFO) << "Closing session " << id_ << " requested";
        state_ = Closing;
        browser_->GetHost()->CloseBrowser(true);
        imageCompressor_->flush();
    } else if(state_ == Pending) {
        LOG(INFO)
            << "Closing session " << id_ << " requested "
            << "while session is still opening, deferring request";

        // Close the browser as soon as it opens
        closeOnOpen_ = true;
    }
}

void Session::handleHTTPRequest(shared_ptr<HTTPRequest> request) {
    requireUIThread();

    if(state_ == Closing || state_ == Closed) {
        request->sendTextResponse(503, "ERROR: Browser session has been closed");
        return;
    }

    // Force update security status every once in a while just to make sure we
    // don't miss updates for a long time
    if(duration_cast<milliseconds>(
        steady_clock::now() - lastSecurityStatusUpdateTime_
    ).count() >= 1000) {
        updateSecurityStatus_();
    }

    string method = request->method();
    string path = request->path();
    smatch match;

    if(method == "GET" && regex_match(path, match, imagePathRegex)) {
        CHECK(match.size() >= 8);
        optional<uint64_t> mainIdx = parseString<uint64_t>(match[1]);
        optional<uint64_t> imgIdx = parseString<uint64_t>(match[2]);
        optional<int> immediate = parseString<int>(match[3]);
        optional<int> width = parseString<int>(match[4]);
        optional<int> height = parseString<int>(match[5]);
        optional<uint64_t> startEventIdx = parseString<uint64_t>(match[6]);

        if(mainIdx && imgIdx && immediate && width && height && startEventIdx) {
            if(*mainIdx != curMainIdx_ || *imgIdx <= curImgIdx_) {
                request->sendTextResponse(400, "ERROR: Outdated request");
            } else {
                updateInactivityTimeout_();

                handleEvents_(*startEventIdx, match[7].first, match[7].second);
                curImgIdx_ = *imgIdx;
                updateRootViewportSize_(*width, *height);
                if(*immediate) {
                    imageCompressor_->sendCompressedImageNow(request);
                } else {
                    imageCompressor_->sendCompressedImageWait(request);
                }
            }
            return;
        }
    }

    if(method == "GET" && regex_match(path, match, iframePathRegex)) {
        CHECK(match.size() == 2);
        optional<uint64_t> mainIdx = parseString<uint64_t>(match[1]);
        if(mainIdx) {
            if(*mainIdx != curMainIdx_) {
                request->sendTextResponse(400, "ERROR: Outdated request");
            } else if(iframeQueue_.empty()) {
                request->sendTextResponse(200, "OK");
            } else {
                updateInactivityTimeout_();

                function<void(shared_ptr<HTTPRequest>)> iframe = iframeQueue_.front();
                iframeQueue_.pop();

                if(iframeQueue_.empty()) {
                    setWidthSignal_(WidthSignalNoNewIframe);
                }

                iframe(request);
            }
            return;
        }
    }

    if(method == "GET" && regex_match(path, match, downloadPathRegex)) {
        CHECK(match.size() == 2);
        optional<uint64_t> downloadIdx = parseString<uint64_t>(match[1]);
        if(downloadIdx) {
            auto it = downloads_.find(*downloadIdx);
            if(it == downloads_.end()) {
                request->sendTextResponse(400, "ERROR: Outdated download index");
            } else {
                it->second.first->serve(request);
            }
            return;
        }
    }

    if(method == "GET" && regex_match(path, match, closePathRegex)) {
        CHECK(match.size() == 2);
        optional<uint64_t> mainIdx = parseString<uint64_t>(match[1]);
        if(mainIdx) {
            if(*mainIdx != curMainIdx_) {
                request->sendTextResponse(400, "ERROR: Outdated request");
            } else {
                // Close requested, increment mainIdx to invalidate requests to
                // the current main and set shortened inactivity timer as this
                // may be a reload
                ++curMainIdx_;
                curImgIdx_ = 0;
                curEventIdx_ = 0;
                updateInactivityTimeout_(true);

                request->sendTextResponse(200, "OK");
            }
            return;
        }
    }

    if(method == "GET" && regex_match(path, match, mainPathRegex)) {
        updateInactivityTimeout_();

        if(preMainVisited_) {
            ++curMainIdx_;

            if(curMainIdx_ > 1 && !prevNextClicked_) {
                // This is not first main page load and no prev/next clicked,
                // so this must be a refresh
                browser_->Reload();
            }
            prevNextClicked_ = false;

            // Avoid keys/mouse buttons staying pressed down
            rootWidget_->sendLoseFocusEvent();
            rootWidget_->sendMouseLeaveEvent(0, 0);

            curImgIdx_ = 0;
            curEventIdx_ = 0;
            request->sendHTMLResponse(
                200,
                writeMainHTML,
                {id_, curMainIdx_, validNonCharKeyList}
            );
        } else {
            request->sendHTMLResponse(200, writePreMainHTML, {id_});
            preMainVisited_ = true;
        }
        return;
    }

    if(method == "GET" && regex_match(path, match, prevPathRegex)) {
        updateInactivityTimeout_();

        if(curMainIdx_ > 0 && browser_ && !prevNextClicked_) {
            prevNextClicked_ = true;
            browser_->GoBack();
        }

        if(prePrevVisited_) {
            request->sendHTMLResponse(200, writePrevHTML, {id_});
        } else {
            request->sendHTMLResponse(200, writePrePrevHTML, {id_});
            prePrevVisited_ = true;
        }
        return;
    }

    if(method == "GET" && regex_match(path, match, nextPathRegex)) {
        updateInactivityTimeout_();

        if(curMainIdx_ > 0 && browser_ && !prevNextClicked_) {
            prevNextClicked_ = true;
            browser_->GoForward();
        }

        request->sendHTMLResponse(200, writeNextHTML, {id_});
        return;
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method");
}

uint64_t Session::id() {
    requireUIThread();
    return id_;
}

void Session::onWidgetViewDirty() {
    requireUIThread();

    shared_ptr<Session> self = shared_from_this();
    postTask([self]() {
        self->rootWidget_->render();
        self->sendViewportToCompressor_();
    });
}

void Session::onWidgetCursorChanged() {
    requireUIThread();

    shared_ptr<Session> self = shared_from_this();
    postTask([self]() {
        int cursor = self->rootWidget_->cursor();
        CHECK(cursor >= 0 && cursor < CursorTypeCount);
        self->setHeightSignal_(cursor);
    });
}

void Session::onGlobalHotkeyPressed(GlobalHotkey key) {
    requireUIThread();

    shared_ptr<Session> self = shared_from_this();
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
    });
}

void Session::onAddressSubmitted(string url) {
    requireUIThread();

    if(!browser_) return;

    if(!url.empty()) {
        CefRefPtr<CefFrame> frame = browser_->GetMainFrame();
        if(frame) {
            frame->LoadURL(url);
            rootWidget_->browserArea()->takeFocus();
        }
    }
}

void Session::onQualityChanged(int quality) {
    requireUIThread();
    imageCompressor_->setQuality(quality);
}

void Session::onPendingDownloadAccepted() {
    requireUIThread();
    downloadManager_->acceptPendingDownload();
}

void Session::onFind(string text, bool forward, bool findNext) {
    requireUIThread();
    if(!browser_) return;

    browser_->GetHost()->Find(0, text, forward, false, findNext);
}

void Session::onStopFind(bool clearSelection) {
    requireUIThread();
    if(!browser_) return;

    browser_->GetHost()->StopFinding(clearSelection);
}

void Session::onClipboardButtonPressed() {
    requireUIThread();

    addIframe_([](shared_ptr<HTTPRequest> request) {
        request->sendHTMLResponse(200, writeClipboardIframeHTML, {});
    });
}

void Session::onBrowserAreaViewDirty() {
    requireUIThread();
    sendViewportToCompressor_();
}

void Session::onPendingDownloadCountChanged(int count) {
    requireUIThread();
    rootWidget_->controlBar()->setPendingDownloadCount(count);
}

void Session::onDownloadProgressChanged(vector<int> progress) {
    requireUIThread();
    rootWidget_->controlBar()->setDownloadProgress(move(progress));
}

void Session::onDownloadCompleted(shared_ptr<CompletedDownload> file) {
    requireUIThread();

    weak_ptr<Session> selfWeak = shared_from_this();
    addIframe_([file, selfWeak](shared_ptr<HTTPRequest> request) {
        requireUIThread();

        shared_ptr<Session> self = selfWeak.lock();
        if(!self) return;

        // Some browser use multiple requests to download a file. Thus, we add
        // the file to downloads_ to be kept a certain period of time, and
        // forward the client to the actual download page
        uint64_t downloadIdx = ++self->curDownloadIdx_;
        shared_ptr<Timeout> timeout = Timeout::create(10000);
        CHECK(self->downloads_.insert({downloadIdx, {file, timeout}}).second);

        timeout->set([selfWeak, downloadIdx]() {
            requireUIThread();
            if(shared_ptr<Session> self = selfWeak.lock()) {
                self->downloads_.erase(downloadIdx);
            }
        });

        request->sendHTMLResponse(
            200, writeDownloadIframeHTML, {self->id_, downloadIdx, file->name()}
        );
    });
}

void Session::afterConstruct_(shared_ptr<Session> self) {
    rootWidget_ = RootWidget::create(self, self, self, allowPNG_);
    rootWidget_->setViewport(rootViewport_);

    downloadManager_ = DownloadManager::create(self);

    if(!isPopup_) {
        CefRefPtr<CefClient> client = new Client(self);

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
            LOG(INFO) << "Opening browser for session " << id_ << " failed, closing session";
            state_ = Closed;
            postTask(eventHandler_, &SessionEventHandler::onSessionClosed, id_);
        }
    }

    updateInactivityTimeout_();
}

void Session::updateInactivityTimeout_(bool shortened) {
    requireUIThread();

    inactivityTimeoutLong_->clear(false);
    inactivityTimeoutShort_->clear(false);

    if(state_ == Pending || state_ == Open) {
        shared_ptr<Timeout> timeout =
            shortened ? inactivityTimeoutShort_ : inactivityTimeoutLong_;

        weak_ptr<Session> self = shared_from_this();
        timeout->set([self, shortened]() {
            requireUIThread();
            if(shared_ptr<Session> session = self.lock()) {
                if(session->state_ == Pending || session->state_ == Open) {
                    LOG(INFO)
                        << "Inactivity timeout for session " << session->id_ << " reached"
                        << (shortened ? " (shortened due to client close signal)" : "");
                    session->close();
                }
            }
        });
    }
}

void Session::updateSecurityStatus_() {
    requireUIThread();

    lastSecurityStatusUpdateTime_ = steady_clock::now();

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

void Session::updateRootViewportSize_(int width, int height) {
    requireUIThread();

    width = max(min(width, 4096), 64);
    height = max(min(height, 4096), 64);

    if(rootViewport_.width() != width || rootViewport_.height() != height) {
        paddedRootViewport_ = ImageSlice::createImage(
            width + WidthSignalModulus - 1,
            height + HeightSignalModulus - 1
        );
        rootViewport_ = paddedRootViewport_.subRect(0, width, 0, height);
        rootWidget_->setViewport(rootViewport_);
    }
}

void Session::sendViewportToCompressor_() {
    CHECK(widthSignal_ >= 0 && widthSignal_ < WidthSignalModulus);
    CHECK(heightSignal_ >= 0 && heightSignal_ < HeightSignalModulus);

    int width = paddedRootViewport_.width();
    while(width % WidthSignalModulus != widthSignal_) {
        --width;
    }

    int height = paddedRootViewport_.height();
    while(height % HeightSignalModulus != heightSignal_) {
        --height;
    }

    imageCompressor_->updateImage(
        paddedRootViewport_.subRect(0, width, 0, height)
    );
}

void Session::handleEvents_(
    uint64_t startIdx,
    string::const_iterator begin,
    string::const_iterator end
) {
    uint64_t eventIdx = startIdx;
    if(eventIdx > curEventIdx_) {
        LOG(WARNING) << eventIdx - curEventIdx_ << " events skipped in session " << id_;
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
                LOG(WARNING) << "Could not parse event '" << string(eventBegin, eventEnd) << "' in session " << id_;
            }
            ++eventIdx;
            curEventIdx_ = eventIdx;
        } else {
            ++eventIdx;
        }
    }
}

void Session::setWidthSignal_(int newWidthSignal) {
    if(newWidthSignal != widthSignal_) {
        widthSignal_ = newWidthSignal;
        sendViewportToCompressor_();
    }
}

void Session::setHeightSignal_(int newHeightSignal) {
    if(newHeightSignal != heightSignal_) {
        heightSignal_ = newHeightSignal;
        sendViewportToCompressor_();
    }
}

void Session::addIframe_(function<void(shared_ptr<HTTPRequest>)> iframe) {
    iframeQueue_.push(iframe);
    setWidthSignal_(WidthSignalNewIframe);
}
