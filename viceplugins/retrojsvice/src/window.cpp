#include "window.hpp"

#include "download.hpp"
#include "gui.hpp"
#include "html.hpp"
#include "http.hpp"
#include "key.hpp"
#include "secrets.hpp"

namespace retrojsvice {

namespace {

regex imagePathRegex(
    "/image/([0-9]+)/([0-9]+)/([01])/([0-9]+)/([0-9]+)/([0-9]+)/(([A-Z0-9_-]+/)*)"
);
regex iframePathRegex(
    "/iframe/([0-9]+)/[0-9]+/"
);
regex downloadPathRegex(
    "/download/([0-9]+)/.*"
);
regex closePathRegex(
    "/close/([0-9]+)/"
);

}

Window::Window(CKey,
    shared_ptr<WindowEventHandler> eventHandler,
    uint64_t handle,
    shared_ptr<SecretGenerator> secretGen,
    string programName,
    bool allowPNG,
    int initialQuality
) {
    REQUIRE_API_THREAD();
    REQUIRE(handle);
    REQUIRE(initialQuality >= 10 && initialQuality <= 101);

    if(!allowPNG && initialQuality == 101) {
        initialQuality = 100;
    }

    programName_ = move(programName);
    allowPNG_ = allowPNG;
    initialQuality_ = initialQuality;
    secretGen_ = secretGen;
    snakeOilKeyCipherKey_ = secretGen_->generateSnakeOilCipherKey();

    eventHandler_ = eventHandler;
    handle_ = handle;
    csrfToken_ = secretGen->generateCSRFToken();
    pathPrefix_ = "/" + toString(handle) + "/" + csrfToken_;
    closed_ = false;

    width_ = -1;
    height_ = -1;

    prePrevVisited_ = false;
    preMainVisited_ = false;
    prevNextClicked_ = false;

    curMainIdx_ = 0;
    curImgIdx_ = 0;
    curEventIdx_ = 0;
    curDownloadIdx_ = 0;

    lastNavigateOperationTime_ = steady_clock::now();

    inFileUploadMode_ = false;

    // Initialization is completed in afterConstruct_
}

Window::~Window() {
    REQUIRE(closed_);
}

void Window::close() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    closed_ = true;

    // stopFetching will make sure that imageCompressor_->flush will not call
    // event handlers
    imageCompressor_->stopFetching();
    imageCompressor_->flush(mce);

    REQUIRE(eventHandler_);
    eventHandler_.reset();

    while(!iframeQueue_.empty()) {
        iframeQueue_.pop();
    }
    downloads_.clear();
}

void Window::handleInitialForwardHTTPRequest(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();

    if(closed_) {
        request->sendTextResponse(400, "ERROR: Window has been closed\n");
        return;
    }

    request->sendHTMLResponse(
        200, writeNewWindowHTML, {programName_, pathPrefix_}
    );
}

void Window::handleHTTPRequest(MCE, shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();

    string fullPath = request->path();
    if(
        fullPath.size() < pathPrefix_.size() ||
        !passwordsEqual(fullPath.substr(0, pathPrefix_.size()), pathPrefix_)
    ) {
        request->sendTextResponse(403, "ERROR: Invalid CSRF token\n");
        return;
    }
    string path = fullPath.substr(pathPrefix_.size());

    if(closed_) {
        request->sendTextResponse(400, "ERROR: Window has been closed\n");
        return;
    }

    string method = request->method();
    smatch match;

    if(method == "GET" && path == "/") {
        handleMainPageRequest_(mce, request);
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
                mce,
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

    if(method == "GET" && regex_match(path, match, iframePathRegex)) {
        REQUIRE(match.size() == 2);
        optional<uint64_t> mainIdx = parseString<uint64_t>(match[1]);

        if(mainIdx) {
            handleIframeRequest_(mce, request, *mainIdx);
            return;
        }
    }

    if(method == "GET" && regex_match(path, match, downloadPathRegex)) {
        REQUIRE(match.size() == 2);
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
        REQUIRE(match.size() == 2);
        optional<uint64_t> mainIdx = parseString<uint64_t>(match[1]);

        if(mainIdx) {
            handleCloseRequest_(request, *mainIdx);
            return;
        }
    }

    if(method == "GET" && path == "/prev/") {
        handlePrevPageRequest_(mce, request);
        return;
    }
    if(method == "GET" && path == "/next/") {
        handleNextPageRequest_(mce, request);
        return;
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method");
}

shared_ptr<Window> Window::createPopup(uint64_t popupHandle) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);
    REQUIRE(popupHandle);
    REQUIRE(eventHandler_);

    shared_ptr<Window> popupWindow = Window::create(
        eventHandler_,
        popupHandle,
        secretGen_,
        programName_,
        allowPNG_,
        imageCompressor_->quality()
    );

    shared_ptr<Window> self = shared_from_this();
    postTask([self, popupWindow]() {
        if(self->closed_ || popupWindow->closed_) {
            return;
        }
        self->addIframe_(mce,
            [self, popupWindow](shared_ptr<HTTPRequest> request) {
                request->sendHTMLResponse(
                    200,
                    writePopupIframeHTML,
                    {self->programName_, popupWindow->pathPrefix_}
                );
            }
        );
    });

    return popupWindow;
}

void Window::notifyViewChanged() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    shared_ptr<Window> self = shared_from_this();
    postTask([self]() {
        if(!self->closed_) {
            self->imageCompressor_->updateNotify(mce);
        }
    });
}

void Window::setCursor(int cursorSignal) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);
    REQUIRE(
        cursorSignal >= 0 && cursorSignal < ImageCompressor::CursorSignalCount
    );

    if(inFileUploadMode_) {
        cursorSignal = ImageCompressor::CursorSignalNormal;
    }

    shared_ptr<Window> self = shared_from_this();
    postTask([self, cursorSignal]() {
        if(!self->closed_) {
            self->imageCompressor_->setCursorSignal(mce, cursorSignal);
        }
    });
}

optional<pair<vector<string>, size_t>> Window::qualitySelectorQuery() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    vector<string> labels;
    for(int i = 10; i <= 100; ++i) {
        labels.push_back(toString(i));
    }
    if(allowPNG_) {
        labels.push_back("PNG");
    }
    return pair<vector<string>, size_t>(
        move(labels), (size_t)(imageCompressor_->quality() - 10)
    );
}

void Window::qualityChanged(size_t qualityIdx) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    int quality = (int)qualityIdx + 10;
    REQUIRE(quality >= 10);
    REQUIRE(quality <= (allowPNG_ ? 101 : 100));

    postTask([quality, imageCompressor{imageCompressor_}]() {
        imageCompressor->setQuality(mce, quality);
    });
}

void Window::clipboardButtonPressed() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    shared_ptr<Window> self = shared_from_this();
    postTask([self]() {
        if(self->closed_) {
            return;
        }
        self->addIframe_(mce,
            [self](shared_ptr<HTTPRequest> request) {
                request->sendHTMLResponse(
                    200,
                    writeClipboardIframeHTML,
                    {self->programName_}
                );
            }
        );
    });
}

void Window::putFileDownload(shared_ptr<FileDownload> file) {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);

    shared_ptr<Window> self = shared_from_this();
    postTask([self, file]() {
        if(self->closed_) {
            return;
        }

        self->addIframe_(mce, [self, file](shared_ptr<HTTPRequest> request) {
            // Some browsers use multiple requests to download a file. Thus, we
            // add the file to downloads_ to be kept a certain period of time,
            // and forward the client to the actual download page.
            uint64_t downloadIdx = ++self->curDownloadIdx_;

            shared_ptr<DelayedTaskTag> tag = postDelayedTask(
                milliseconds(10000),
                [self, downloadIdx]() {
                    self->downloads_.erase(downloadIdx);
                }
            );
            REQUIRE(self->downloads_.insert({downloadIdx, {file, tag}}).second);

            request->sendHTMLResponse(
                200, writeDownloadIframeHTML, {
                    self->programName_,
                    self->pathPrefix_,
                    downloadIdx,
                    file->name()
                }
            );
        });
    });
}

bool Window::startFileUpload() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);
    REQUIRE(!inFileUploadMode_);

    inFileUploadMode_ = true;
    setCursor(ImageCompressor::CursorSignalNormal);
    notifyViewChanged();

    return true;
}

void Window::cancelFileUpload() {
    REQUIRE_API_THREAD();
    REQUIRE(!closed_);
    REQUIRE(inFileUploadMode_);

    inFileUploadMode_ = false;
    notifyViewChanged();
}

void Window::onImageCompressorFetchImage(
    function<void(const uint8_t*, size_t, size_t, size_t)> func
) {
    REQUIRE_API_THREAD();

    if(closed_) {
        vector<uint8_t> data(4, (uint8_t)255);
        func(data.data(), 1, 1, 1);
    } else {
        REQUIRE(eventHandler_);
        eventHandler_->onWindowFetchImage(handle_, func);
    }
}

void Window::onImageCompressorRenderGUI(
    vector<uint8_t>& data, size_t width, size_t height
) {
    REQUIRE_API_THREAD();

    if(!closed_ && inFileUploadMode_) {
        renderUploadModeGUI(data, width, height);
    }
}

void Window::afterConstruct_(shared_ptr<Window> self) {
    imageCompressor_ = ImageCompressor::create(
        self, milliseconds(2000), initialQuality_
    );

    updateInactivityTimeout_();
    notifyViewChanged();
}

void Window::selfClose_(MCE) {
    shared_ptr<WindowEventHandler> eventHandler = eventHandler_;
    close();

    REQUIRE(eventHandler);
    eventHandler->onWindowClose(handle_);
}

void Window::updateInactivityTimeout_(bool shorten) {
    REQUIRE_API_THREAD();
    if(closed_) return;

    inactivityTimeoutTag_ = postDelayedTask(
        milliseconds(shorten ? 4000 : 30000),
        weak_ptr<Window>(shared_from_this()),
        &Window::inactivityTimeoutReached_,
        mce,
        shorten
    );
}

void Window::inactivityTimeoutReached_(MCE, bool shortened) {
    REQUIRE_API_THREAD();
    if(closed_) return;

    INFO_LOG(
        "Closing window ", handle_, " due to inactivity timeout",
        (shortened ? " (shortened due to client close signal)" : "")
    );
    selfClose_(mce);
}

int Window::decodeKey_(uint64_t eventIdx, int key) {
    REQUIRE(!snakeOilKeyCipherKey_.empty());
    size_t i = (size_t)eventIdx % snakeOilKeyCipherKey_.size();
    return key ^ snakeOilKeyCipherKey_[i];
}

bool Window::handleTokenizedEvent_(MCE,
    uint64_t eventIdx,
    const string& name,
    int argCount,
    const int* args
) {
    REQUIRE(eventHandler_);

    auto keyDown = [&](int key) {
        keysDown_.insert(key);
        eventHandler_->onWindowKeyDown(handle_, key);
    };
    auto keyUp = [&](int key) {
        if(keysDown_.erase(key)) {
            eventHandler_->onWindowKeyUp(handle_, key);
        }
    };

    if(name == "MUP" && argCount == 3 && args[2] >= 0 && args[2] <= 2) {
        int x = args[0];
        int y = args[1];
        int button = args[2];
        if(mouseButtonsDown_.erase(button)) {
            eventHandler_->onWindowMouseUp(handle_, x, y, button);
        }
        eventHandler_->onWindowMouseMove(handle_, x, y);
        return true;
    }
    if(name == "KUP" && argCount == 1) {
        int key = -decodeKey_(eventIdx, args[0]);
        if(key < 0 && isValidKey(key)) {
            keyUp(key);
        }
        return true;
    }

    if(inFileUploadMode_) {
        // All the events after this are such that we can safely ignore them in
        // file upload mode to make it modal.
        return true;
    }

    if(name == "MDN" && argCount == 3 && args[2] >= 0 && args[2] <= 2) {
        int x = args[0];
        int y = args[1];
        int button = args[2];
        if(mouseButtonsDown_.insert(button).second) {
            eventHandler_->onWindowMouseDown(handle_, x, y, button);
        }
        eventHandler_->onWindowMouseMove(handle_, x, y);
        return true;
    }
    if(name == "MDBL" && argCount == 2) {
        int x = args[0];
        int y = args[1];
        eventHandler_->onWindowMouseDoubleClick(handle_, x, y, 0);
        return true;
    }
    if(name == "MWH" && argCount == 3) {
        int x = args[0];
        int y = args[1];
        int delta = args[2];
        delta = max(-180, min(180, delta));
        eventHandler_->onWindowMouseWheel(handle_, x, y, delta);
        return true;
    }
    if(name == "MMO" && argCount == 2) {
        int x = args[0];
        int y = args[1];
        eventHandler_->onWindowMouseMove(handle_, x, y);
        return true;
    }
    if(name == "MOUT" && argCount == 2) {
        int x = args[0];
        int y = args[1];
        eventHandler_->onWindowMouseLeave(handle_, x, y);
        return true;
    }

    if(name == "KDN" && argCount == 1) {
        int key = -decodeKey_(eventIdx, args[0]);
        if(key < 0 && isValidKey(key)) {
            keyDown(key);
        }
        return true;
    }
    if(name == "KPR" && argCount == 1) {
        int key = decodeKey_(eventIdx, args[0]);
        if(key > 0 && isValidKey(key)) {
            keyDown(key);
            keyUp(key);
        }
        return true;
    }
    if(name == "FOUT" && argCount == 0) {
        eventHandler_->onWindowLoseFocus(handle_);
        return true;
    }
    return false;
}

bool Window::handleEvent_(MCE,
    uint64_t eventIdx,
    string::const_iterator begin,
    string::const_iterator end
) {
    REQUIRE(begin < end && *(end - 1) == '/');

    string name;

    const int MaxArgCount = 3;
    int args[MaxArgCount];
    int argCount = 0;

    string::const_iterator pos = begin;
    while(*pos != '/' && *pos != '_') {
        ++pos;
    }
    name = string(begin, pos);

    bool ok = true;
    if(*pos == '_') {
        ++pos;
        while(true) {
            if(argCount == MaxArgCount) {
                ok = false;
                break;
            }
            string::const_iterator argStart = pos;
            while(*pos != '/' && *pos != '_') {
                ++pos;
            }
            optional<int> arg = parseString<int>(string(argStart, pos));
            if(!arg) {
                ok = false;
                break;
            }
            args[argCount++] = *arg;
            if(*pos == '/') {
                break;
            }
            ++pos;
        }
    }

    return ok && handleTokenizedEvent_(mce, eventIdx, name, argCount, args);
}

void Window::handleEvents_(MCE, uint64_t startIdx, string eventStr) {
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
            if(!handleEvent_(mce, eventIdx, itemBegin, itemEnd)) {
                WARNING_LOG(
                    "Could not parse event '", string(itemBegin, itemEnd),
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

void Window::navigate_(MCE, int direction) {
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
        REQUIRE(eventHandler_);
        eventHandler_->onWindowNavigate(handle_, direction);
    }
}

void Window::handleMainPageRequest_(MCE, shared_ptr<HTTPRequest> request) {
    updateInactivityTimeout_();

    if(preMainVisited_) {
        ++curMainIdx_;

        if(curMainIdx_ > 1 && !prevNextClicked_) {
            // This is not first main page load and no prev/next clicked,
            // so this must be a refresh
            navigate_(mce, 0);
        }
        prevNextClicked_ = false;

        if(curMainIdx_ > 1) {
            // Make sure that no mouse buttons or keys are stuck down and the
            // focus and mouseover state is reset
            REQUIRE(eventHandler_);
            while(!mouseButtonsDown_.empty()) {
                int button = *mouseButtonsDown_.begin();
                mouseButtonsDown_.erase(mouseButtonsDown_.begin());
                eventHandler_->onWindowMouseUp(handle_, 0, 0, button);
            }
            while(!keysDown_.empty()) {
                int key = *keysDown_.begin();
                keysDown_.erase(keysDown_.begin());
                eventHandler_->onWindowKeyUp(handle_, key);
            }
            eventHandler_->onWindowMouseLeave(handle_, 0, 0);
            eventHandler_->onWindowLoseFocus(handle_);
        }

        snakeOilKeyCipherKey_ = secretGen_->generateSnakeOilCipherKey();
        stringstream snakeOilKeyStream;
        const int ChunkSize = 30;
        for(int i = 0; i < (int)snakeOilKeyCipherKey_.size(); i += ChunkSize) {
            if(i != 0) {
                snakeOilKeyStream << "\n";
            }
            snakeOilKeyStream << "pushSnakeOil(new Array(";
            for(
                int j = i;
                j < (int)snakeOilKeyCipherKey_.size() && j < i + ChunkSize;
                ++j
            ) {
                if(j != i) {
                    snakeOilKeyStream << ',';
                }
                snakeOilKeyStream << snakeOilKeyCipherKey_[j];
            }
            snakeOilKeyStream << "));";
        }
        string snakeOilKeyCipherKeyWrites = snakeOilKeyStream.str();

        curImgIdx_ = 0;
        curEventIdx_ = 0;
        request->sendHTMLResponse(200, writeMainHTML, {
            programName_,
            pathPrefix_,
            curMainIdx_,
            validNonCharKeyList,
            snakeOilKeyCipherKeyWrites
        });
    } else {
        request->sendHTMLResponse(
            200, writePreMainHTML, {programName_, pathPrefix_}
        );
        preMainVisited_ = true;
    }
}

void Window::handleImageRequest_(MCE,
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

        handleEvents_(mce, startEventIdx, move(eventStr));
        curImgIdx_ = imgIdx;

        width = min(max(width, 1), 16384);
        height = min(max(height, 1), 16384);

        if(width != width_ || height != height_) {
            width_ = width;
            height_ = height;

            REQUIRE(eventHandler_);
            eventHandler_->onWindowResize(
                handle_,
                (size_t)width,
                (size_t)height
            );
        }

        if(immediate) {
            imageCompressor_->sendCompressedImageNow(mce, request);
        } else {
            imageCompressor_->sendCompressedImageWait(mce, request);
        }
    }
}

void Window::handleIframeRequest_(MCE,
    shared_ptr<HTTPRequest> request,
    uint64_t mainIdx
) {
    if(mainIdx != curMainIdx_) {
        request->sendTextResponse(400, "ERROR: Outdated request");
    } else if(iframeQueue_.empty()) {
        request->sendTextResponse(200, "OK");
    } else {
        updateInactivityTimeout_();

        function<void(shared_ptr<HTTPRequest>)> iframe = iframeQueue_.front();
        iframeQueue_.pop();

        if(iframeQueue_.empty()) {
            imageCompressor_->setIframeSignal(
                mce, ImageCompressor::IframeSignalFalse
            );
        }

        iframe(request);
    }
}

void Window::handleCloseRequest_(
    shared_ptr<HTTPRequest> request,
    uint64_t mainIdx
) {
    if(mainIdx != curMainIdx_) {
        request->sendTextResponse(400, "ERROR: Outdated request");
    } else {
        // Close requested, increment mainIdx to invalidate requests to the
        // current main and set shortened inactivity timer as this may be a
        // reload
        ++curMainIdx_;
        curImgIdx_ = 0;
        curEventIdx_ = 0;
        updateInactivityTimeout_(true);

        request->sendTextResponse(200, "OK");
    }
}

void Window::handlePrevPageRequest_(MCE, shared_ptr<HTTPRequest> request) {
    updateInactivityTimeout_();

    if(curMainIdx_ > 0 && !prevNextClicked_) {
        prevNextClicked_ = true;
        navigate_(mce, -1);
    }

    if(prePrevVisited_) {
        request->sendHTMLResponse(
            200, writePrevHTML, {programName_, pathPrefix_}
        );
    } else {
        request->sendHTMLResponse(
            200, writePrePrevHTML, {programName_, pathPrefix_}
        );
        prePrevVisited_ = true;
    }
}

void Window::handleNextPageRequest_(MCE, shared_ptr<HTTPRequest> request) {
    updateInactivityTimeout_();

    if(curMainIdx_ > 0 && !prevNextClicked_) {
        prevNextClicked_ = true;
        navigate_(mce, 1);
    }

    request->sendHTMLResponse(200, writeNextHTML, {programName_, pathPrefix_});
    return;
}

void Window::addIframe_(MCE, function<void(shared_ptr<HTTPRequest>)> iframe) {
    REQUIRE(!closed_);

    iframeQueue_.push(iframe);
    imageCompressor_->setIframeSignal(mce, ImageCompressor::IframeSignalTrue);
}

}
