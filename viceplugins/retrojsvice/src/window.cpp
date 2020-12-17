#include "window.hpp"

#include "html.hpp"
#include "http.hpp"

namespace retrojsvice {

namespace {

regex mainPathRegex("/[0-9]+/");
regex prevPathRegex("/[0-9]+/prev/");
regex nextPathRegex("/[0-9]+/next/");

}

Window::Window(CKey, shared_ptr<WindowEventHandler> eventHandler, uint64_t handle) {
    REQUIRE_API_THREAD();
    REQUIRE(handle);

    eventHandler_ = eventHandler;
    handle_ = handle;
    closed_ = false;

    prePrevVisited_ = false;
    preMainVisited_ = false;
    prevNextClicked_ = false;

    curMainIdx_ = 0;
    curImgIdx_ = 0;
    curEventIdx_ = 0;

    lastNavigateOperationTime_ = steady_clock::now();
}

Window::~Window() {
    REQUIRE(closed_);
}

void Window::close() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    closed_ = true;

    REQUIRE(eventHandler_);
    eventHandler_->onWindowClose(handle_);

    eventHandler_.reset();
}

void Window::handleHTTPRequest(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    string method = request->method();
    string path = request->path();
    smatch match;

    if(method == "GET" && regex_match(path, match, mainPathRegex)) {
        if(preMainVisited_) {
            ++curMainIdx_;

            if(curMainIdx_ > 1 && !prevNextClicked_) {
                // This is not first main page load and no prev/next clicked,
                // so this must be a refresh
                navigate_(0);
            }
            prevNextClicked_ = false;

            // TODO: Avoid keys/mouse buttons staying pressed down
            //rootWidget_->sendLoseFocusEvent();
            //rootWidget_->sendMouseLeaveEvent(0, 0);

            curImgIdx_ = 0;
            curEventIdx_ = 0;
            request->sendHTMLResponse(
                200,
                writeMainHTML,
                {handle_, curMainIdx_}// TODO:, validNonCharKeyList}
            );
        } else {
            request->sendHTMLResponse(200, writePreMainHTML, {handle_});
            preMainVisited_ = true;
        }
        return;
    }

    // Back/forward button emulation dummy pages
    if(method == "GET" && regex_match(path, match, prevPathRegex)) {
        if(curMainIdx_ > 0 && !prevNextClicked_) {
            prevNextClicked_ = true;
            navigate_(-1);
        }

        if(prePrevVisited_) {
            request->sendHTMLResponse(200, writePrevHTML, {handle_});
        } else {
            request->sendHTMLResponse(200, writePrePrevHTML, {handle_});
            prePrevVisited_ = true;
        }
        return;
    }
    if(method == "GET" && regex_match(path, match, nextPathRegex)) {
        if(curMainIdx_ > 0 && !prevNextClicked_) {
            prevNextClicked_ = true;
            navigate_(1);
        }

        request->sendHTMLResponse(200, writeNextHTML, {handle_});
        return;
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

    if(!closed_) {
        INFO_LOG("TODO: navigate_(", direction, ")");
    }
}

}
