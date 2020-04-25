#include "session.hpp"

#include "browser_area.hpp"
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
regex imagePathRegex("/[0-9]+/image/([0-9]+)/([0-9]+)/");

}

class Session::Client :
    public CefClient,
    public CefLifeSpanHandler
{
public:
    Client(shared_ptr<Session> session) {
        session_ = session;
        renderHandler_ =
            session->rootWidget_->browserArea()->createCefRenderHandler();
    }

    // CefClient:
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return renderHandler_;
    }

    // CefLifeSpanHandler:
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
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
        CEF_REQUIRE_UI_THREAD();
        CHECK(session_->state_ == Open || session_->state_ == Closing);

        session_->state_ = Closed;
        session_->browser_ = nullptr;
        session_->rootWidget_->browserArea()->setBrowser(nullptr);
        session_->imageCompressor_->flush();

        LOG(INFO) << "Session " << session_->id_ << " closed";

        postTask(session_->eventHandler_, &SessionEventHandler::onSessionClosed, session_->id_);
        session_->updateInactivityTimeout_();
    }

private:
    shared_ptr<Session> session_;

    CefRefPtr<CefRenderHandler> renderHandler_;

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

    rootViewport_ = ImageSlice::createImage(800, 600);

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
    CEF_REQUIRE_UI_THREAD();

    if(state_ == Closing || state_ == Closed) {
        request->sendTextResponse(503, "ERROR: Browser session has been closed");
        return;
    }

    updateInactivityTimeout_();

    string method = request->method();
    string path = request->path();
    smatch match;

    if(method == "GET" && regex_match(path, match, imagePathRegex)) {
        CHECK(match.size() == 3);
        optional<int> width = parseString<int>(match[1]);
        optional<int> height = parseString<int>(match[2]);

        if(width && height) {
            updateRootViewportSize_(*width, *height);
            imageCompressor_->sendCompressedImageWait(request);
            return;
        }
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
    imageCompressor_->updateImage(rootViewport_);
}

void Session::afterConstruct_(shared_ptr<Session> self) {
    rootWidget_ = RootWidget::create(self);
    rootWidget_->setViewport(rootViewport_);

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

void Session::updateRootViewportSize_(int width, int height) {
    CEF_REQUIRE_UI_THREAD();

    width = max(min(width, 4096), 64);
    height = max(min(height, 4096), 64);

    if(rootViewport_.width() != width || rootViewport_.height() != height) {
        rootViewport_ = ImageSlice::createImage(width, height);
        rootWidget_->setViewport(rootViewport_);
    }
}
