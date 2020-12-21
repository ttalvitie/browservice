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

    imageCompressor_->flush();

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

void Window::onFetchImage(
    function<void(const uint8_t*, size_t, size_t, size_t)> func
) {
    REQUIRE_API_THREAD();

    uint8_t shift = (uint8_t)(duration_cast<milliseconds>(
        steady_clock::now() - lastNavigateOperationTime_
    ).count() >> 5);

    size_t width = 512;
    size_t height = 256;
    size_t pitch = 564;
    vector<uint8_t> data(4 * pitch * height);
    for(size_t y = 0; y < height; ++y) {
        for(size_t x = 0; x < width; ++x) {
            for(size_t c = 0; c < 3; ++c) {
                data[4 * (y * pitch + x) + c] = (uint8_t)((uint8_t)x + shift) ^ (uint8_t)((uint8_t)y + shift);
            }
        }
    }
    func(data.data(), width, height, pitch);
}

void Window::afterConstruct_(shared_ptr<Window> self) {
    imageCompressor_ = ImageCompressor::create(self, milliseconds(2000), true, 100); // TODO set parameters
    animate_();

    updateInactivityTimeout_();
}

void Window::animate_() {
    imageCompressor_->updateNotify();

    int cursorType = max((int)(duration_cast<milliseconds>(
        steady_clock::now() - lastNavigateOperationTime_
    ).count() >> 10), 0) % 3;
    imageCompressor_->setCursorSignal(cursorType);

    if(duration_cast<milliseconds>(
        steady_clock::now() - lastNavigateOperationTime_
    ).count() > 10000) {
        imageCompressor_->setIframeSignal(ImageCompressor::IframeSignalTrue);
    }

    animationTag_ = postDelayedTask(
        milliseconds(300),
        weak_ptr<Window>(shared_from_this()),
        &Window::animate_
    );
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
            imageCompressor_->sendCompressedImageNow(request);
        } else {
            imageCompressor_->sendCompressedImageWait(request);
        }
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
