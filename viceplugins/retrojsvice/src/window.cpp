#include "window.hpp"

#include "html.hpp"
#include "http.hpp"

namespace retrojsvice {

namespace {

regex mainPathRegex("/[0-9]+/");
regex prevPathRegex("/[0-9]+/prev/");
regex nextPathRegex("/[0-9]+/next/");
regex imagePathRegex(
    "/[0-9]+/image/([0-9]+)/([0-9]+)/([01])/([0-9]+)/([0-9]+)/([0-9]+)/(([A-Z0-9_-]+/)*)"
);

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

    // Initialization is completed in afterConstruct_
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

    if(closed_) {
        request->sendTextResponse(400, "ERROR: Window has been closed\n");
        return;
    }

    string method = request->method();
    string path = request->path();
    smatch match;

    if(method == "GET" && regex_match(path, match, mainPathRegex)) {
        handleMainPageRequest_(request);
        return;
    }

    if(method == "GET" && regex_match(path, match, imagePathRegex)) {
        REQUIRE(match.size() >= 8);
        optional<uint64_t> mainIdx = parseString<uint64_t>(match[1]);
        optional<uint64_t> imgIdx = parseString<uint64_t>(match[2]);
        optional<int> immediate = parseString<int>(match[3]);
        optional<int> width = parseString<int>(match[4]);
        optional<int> height = parseString<int>(match[5]);
        optional<uint64_t> startEventIdx = parseString<uint64_t>(match[6]);
        string eventStr = match[7];

        if(mainIdx && imgIdx && immediate && width && height && startEventIdx) {
            handleImageRequest_(
                request,
                *mainIdx,
                *imgIdx,
                *immediate,
                *width,
                *height,
                *startEventIdx,
                move(eventStr)
            );
            return;
        }
    }

    if(method == "GET" && regex_match(path, match, prevPathRegex)) {
        handlePrevPageRequest_(request);
        return;
    }
    if(method == "GET" && regex_match(path, match, nextPathRegex)) {
        handleNextPageRequest_(request);
        return;
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method");
}

void Window::afterConstruct_(shared_ptr<Window> self) {
    updateInactivityTimeout_();
}

void Window::updateInactivityTimeout_(bool shorten) {
    REQUIRE_API_THREAD();
    if(closed_) return;

    inactivityTimeoutTag_ = postDelayedTask(
        milliseconds(shorten ? 4000 : 30000),
        weak_ptr<Window>(shared_from_this()),
        &Window::inactivityTimeoutReached_,
        shorten
    );
}

void Window::inactivityTimeoutReached_(bool shortened) {
    REQUIRE_API_THREAD();
    if(closed_) return;

    INFO_LOG(
        "Closing window ", handle_, " due to inactivity timeout",
        (shortened ? " (shortened due to client close signal)" : "")
    );
    close();
}

void Window::handleEvents_(uint64_t startIdx, string eventStr) {
    REQUIRE_API_THREAD();
    if(closed_) return;

    if(startIdx > ((uint64_t)-1 >> 1)) {
        WARNING_LOG(
            "Too large event index received from client in window ",
            handle_, ", ignoring"
        );
        return;
    }

    uint64_t eventIdx = startIdx;
    if(eventIdx > curEventIdx_) {
        WARNING_LOG(eventIdx - curEventIdx_, " events skipped in window ", handle_);
        curEventIdx_ = eventIdx;
    }

    string::const_iterator begin = eventStr.begin();
    string::const_iterator end = eventStr.end();

    string::const_iterator itemEnd = begin;
    while(true) {
        string::const_iterator itemBegin = itemEnd;
        while(true) {
            if(itemEnd >= end) {
                return;
            }
            if(*itemEnd == '/') {
                ++itemEnd;
                break;
            }
            ++itemEnd;
        }

        if(eventIdx == curEventIdx_) {
            INFO_LOG("TODO: process event ", string(itemBegin, itemEnd));
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

    if(!closed_) {
        INFO_LOG("TODO: navigate_(", direction, ")");
    }
}

void Window::handleMainPageRequest_(shared_ptr<HTTPRequest> request) {
    updateInactivityTimeout_();

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
}

namespace {

void serveWhiteJPEGPixel(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();

    // 1x1 white JPEG
    vector<uint8_t> data = {
        255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 1, 0, 72, 0, 72,
        0, 0, 255, 219, 0, 67, 0, 3, 2, 2, 3, 2, 2, 3, 3, 3, 3, 4, 3, 3, 4,
        5, 8, 5, 5, 4, 4, 5, 10, 7, 7, 6, 8, 12, 10, 12, 12, 11, 10, 11, 11,
        13, 14, 18, 16, 13, 14, 17, 14, 11, 11, 16, 22, 16, 17, 19, 20, 21,
        21, 21, 12, 15, 23, 24, 22, 20, 24, 18, 20, 21, 20, 255, 219, 0, 67,
        1, 3, 4, 4, 5, 4, 5, 9, 5, 5, 9, 20, 13, 11, 13, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 255, 192, 0, 17, 8, 0,
        1, 0, 1, 3, 1, 17, 0, 2, 17, 1, 3, 17, 1, 255, 196, 0, 20, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 255, 196, 0, 20, 16, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 196, 0, 20, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 196, 0, 20,
        17, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 218, 0,
        12, 3, 1, 0, 2, 17, 3, 17, 0, 63, 0, 84, 193, 255, 217
    };
    uint64_t contentLength = data.size();
    request->sendResponse(
        200,
        "image/jpeg",
        contentLength,
        [data{move(data)}](ostream& out) {
            out.write((const char*)data.data(), data.size());
        }
    );
}

}

void Window::handleImageRequest_(
    shared_ptr<HTTPRequest> request,
    uint64_t mainIdx,
    uint64_t imgIdx,
    int immediate,
    int width,
    int height,
    uint64_t startEventIdx,
    string eventStr
) {
    if(mainIdx != curMainIdx_ || imgIdx <= curImgIdx_) {
        request->sendTextResponse(400, "ERROR: Outdated request");
    } else {
        updateInactivityTimeout_();

        handleEvents_(startEventIdx, move(eventStr));
        curImgIdx_ = imgIdx;

        INFO_LOG("TODO: set image size to ", width, "x", height);
        if(immediate) {
            INFO_LOG("TODO: send image immediately");
        } else {
            INFO_LOG("TODO: send image as it becomes available");
        }
        serveWhiteJPEGPixel(request);
    }
}

void Window::handlePrevPageRequest_(shared_ptr<HTTPRequest> request) {
    updateInactivityTimeout_();

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
}

void Window::handleNextPageRequest_(shared_ptr<HTTPRequest> request) {
    updateInactivityTimeout_();

    if(curMainIdx_ > 0 && !prevNextClicked_) {
        prevNextClicked_ = true;
        navigate_(1);
    }

    request->sendHTMLResponse(200, writeNextHTML, {handle_});
    return;
}

}
