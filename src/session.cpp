#include "session.hpp"

#include "html.hpp"
#include "image_compressor.hpp"
#include "timeout.hpp"
#include "root_widget.hpp"

#include "include/cef_client.h"

namespace {

set<uint64_t> usedSessionIDs;
mt19937 sessionIDRNG(random_device{}());

regex mainPathRegex("/[0-9]+/");
regex prevPathRegex("/[0-9]+/prev/");
regex nextPathRegex("/[0-9]+/next/");
regex imagePathRegex("/[0-9]+/image/");

}

class Session::Client :
    public CefClient,
    public CefLifeSpanHandler,
    public CefRenderHandler
{
public:
    Client(shared_ptr<Session> session) {
        session_ = session;
    }

    // CefClient:
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return this;
    }

    // CefLifeSpanHandler:
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        CHECK(session_->state_ == Pending);

        LOG(INFO) << "CEF browser for session " << session_->id_ << " created";

        session_->browser_ = browser;
        session_->state_ = Open;

        if(session_->closeOnOpen_) {
            session_->close();
        }
    }
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        CHECK(session_->state_ == Open || session_->state_ == Closing);

        session_->state_ = Closed;
        session_->browser_ = nullptr;

        LOG(INFO) << "Session " << session_->id_ << " closed";

        postTask(session_->eventHandler_, &SessionEventHandler::onSessionClosed, session_->id_);
        session_->updateInactivityTimeout_();
    }

    // CefRenderHandler:
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        CEF_REQUIRE_UI_THREAD();

        rect.Set(0, 0, 800, 600);
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
        CefRefPtr<CefBrowser> browser,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int bufWidth,
        int bufHeight
    ) override {
        CEF_REQUIRE_UI_THREAD();
    }

private:
    shared_ptr<Session> session_;

    IMPLEMENT_REFCOUNTING(Client);
};

Session::Session(CKey, weak_ptr<SessionEventHandler> eventHandler) {
    CEF_REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;

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

    state_ = Pending;

    closeOnOpen_ = false;

    inactivityTimeout_ = Timeout::create(60000);

    imageCompressor_ = ImageCompressor::create(3000);

    // Initialization is finalized in afterConstruct_
}

Session::~Session() {
    uint64_t id = id_;
    postTask([id]() {
        usedSessionIDs.erase(id);
    });
}

void Session::close() {
    CEF_REQUIRE_UI_THREAD();

    if(state_ == Open) {
        LOG(INFO) << "Closing session " << id_ << " requested";
        state_ = Closing;
        browser_->GetHost()->CloseBrowser(true);
    } else if(state_ == Pending) {
        LOG(INFO)
            << "Closing session " << id_ << " requested "
            << "while session is still opening, deferring request";

        // Close the browser as soon as it opens
        closeOnOpen_ = true;
    }
}

void Session::handleHTTPRequest(shared_ptr<HTTPRequest> request) {
    CEF_REQUIRE_UI_THREAD();

    updateInactivityTimeout_();

    string method = request->method();
    string path = request->path();
    smatch match;

    if(method == "GET" && regex_match(path, match, imagePathRegex)) {
        imageCompressor_->sendCompressedImageNow(request);
        return;
    }

    if(method == "GET" && regex_match(path, match, mainPathRegex)) {
        if(preMainVisited_) {
            request->sendHTMLResponse(200, writeMainHTML, {id_});
        } else {
            request->sendHTMLResponse(200, writePreMainHTML, {id_});
            preMainVisited_ = true;
        }
        return;
    }

    if(method == "GET" && regex_match(path, match, prevPathRegex)) {
        if(prePrevVisited_) {
            request->sendHTMLResponse(200, writePrevHTML, {id_});
        } else {
            request->sendHTMLResponse(200, writePrePrevHTML, {id_});
            prePrevVisited_ = true;
        }
        return;
    }

    if(method == "GET" && regex_match(path, match, nextPathRegex)) {
        request->sendHTMLResponse(200, writeNextHTML, {id_});
        return;
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method");
}

uint64_t Session::id() {
    CEF_REQUIRE_UI_THREAD();
    return id_;
}

void Session::onWidgetViewDirty() {
    CEF_REQUIRE_UI_THREAD();

    rootWidget_->render();
    LOG(INFO) << "Root widget rendered";
}

void Session::afterConstruct_(shared_ptr<Session> self) {
    rootWidget_ = RootWidget::create(self);
    rootWidget_->setViewport(ImageSlice::createImage(800, 600));

    CefRefPtr<CefClient> client = new Client(self);

    CefWindowInfo windowInfo;
    windowInfo.SetAsWindowless(kNullWindowHandle);

    CefBrowserSettings browserSettings;
    browserSettings.background_color = (cef_color_t)-1;

    if(!CefBrowserHost::CreateBrowser(
        windowInfo,
        client,
        "https://cs.helsinki.fi/u/totalvit/baaslinks.html",
        browserSettings,
        nullptr,
        nullptr
    )) {
        LOG(INFO) << "Opening browser for session " << id_ << " failed, closing session";
        state_ = Closed;
        postTask(eventHandler_, &SessionEventHandler::onSessionClosed, id_);
    }

    updateInactivityTimeout_();
}

void Session::updateInactivityTimeout_() {
    CEF_REQUIRE_UI_THREAD();

    inactivityTimeout_->clear(false);

    if(state_ == Pending || state_ == Open) {
        weak_ptr<Session> self = shared_from_this();
        inactivityTimeout_->set([self]() {
            CEF_REQUIRE_UI_THREAD();
            if(shared_ptr<Session> session = self.lock()) {
                if(session->state_ == Pending || session->state_ == Open) {
                    LOG(INFO) << "Inactivity timeout for session " << session->id_ << " reached";
                    session->close();
                }
            }
        });
    }
}
