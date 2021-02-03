#include "image_compressor.hpp"

#include "http.hpp"
#include "jpeg.hpp"
#include "png.hpp"
#include "task_queue.hpp"

namespace retrojsvice {

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

function<void(shared_ptr<HTTPRequest>)> compressPNG_(
    vector<uint8_t> imageData,
    size_t imageWidth,
    size_t imageHeight,
    shared_ptr<PNGCompressor> pngCompressor
) {
    REQUIRE(imageWidth && imageHeight);
    REQUIRE(imageData.size() == 4 * imageWidth * imageHeight);

    shared_ptr<vector<vector<uint8_t>>> png =
        make_shared<vector<vector<uint8_t>>>(
            pngCompressor->compress(
                imageData.data(),
                imageWidth,
                imageHeight,
                imageWidth
            )
        );

    uint64_t length = 0;
    for(const vector<uint8_t>& chunk : *png) {
        length += chunk.size();
    }

    return [png, length](shared_ptr<HTTPRequest> request) {
        REQUIRE_API_THREAD();

        request->sendResponse(
            200,
            "image/png",
            length,
            [png](ostream& out) {
                for(const vector<uint8_t>& chunk : *png) {
                    out.write((const char*)chunk.data(), chunk.size());
                }
            }
        );
    };
}

function<void(shared_ptr<HTTPRequest>)> compressJPEG_(
    vector<uint8_t> imageData,
    size_t imageWidth,
    size_t imageHeight,
    int quality
) {
    REQUIRE(imageWidth && imageHeight);
    REQUIRE(imageData.size() == 4 * imageWidth * imageHeight);
    REQUIRE(quality > 0 && quality <= 100);

    shared_ptr<JPEGData> jpeg = make_shared<JPEGData>(compressJPEG(
        imageData.data(),
        imageWidth,
        imageHeight,
        imageWidth,
        quality
    ));
    return [jpeg](shared_ptr<HTTPRequest> request) {
        REQUIRE_API_THREAD();

        request->sendResponse(
            200,
            "image/jpeg",
            jpeg->length,
            [jpeg](ostream& out) {
                out.write((const char*)jpeg->data.get(), jpeg->length);
            }
        );
    };
}

}

ImageCompressor::ImageCompressor(CKey,
    weak_ptr<ImageCompressorEventHandler> eventHandler,
    steady_clock::duration sendTimeout,
    int quality
) {
    REQUIRE_API_THREAD();
    REQUIRE(quality >= 10 && quality <= 101);

    eventHandler_ = eventHandler;
    sendTimeout_ = sendTimeout;

    quality_ = quality;

    iframeSignal_ = 1;
    cursorSignal_ = 1;

    int pngThreadCount = (int)thread::hardware_concurrency();
    pngThreadCount = min(pngThreadCount, 4);
    pngThreadCount = max(pngThreadCount, 1);
    pngCompressor_ = make_shared<PNGCompressor>(pngThreadCount);

    compressorShutdownScheduled_ = false;
    compressorTaskScheduled_ = false;

    compressedImage_ = serveWhiteJPEGPixel;

    fetchingStopped_ = false;
    imageUpdated_ = false;
    compressedImageUpdated_ = false;
    compressionInProgress_ = false;
}

ImageCompressor::~ImageCompressor() {
    {
        lock_guard<mutex> lock(compressorMutex_);
        compressorShutdownScheduled_ = true;
    }
    compressorCv_.notify_one();
    compressorThread_.join();
}

int ImageCompressor::quality() {
    REQUIRE_API_THREAD();
    return quality_;
}

void ImageCompressor::setQuality(MCE, int quality) {
    REQUIRE_API_THREAD();
    REQUIRE(quality >= 10 && quality <= 101);

    if(quality != quality_) {
        quality_ = quality;
        updateNotify(mce);
    }
}

void ImageCompressor::updateNotify(MCE) {
    REQUIRE_API_THREAD();

    imageUpdated_ = true;
    pump_(mce);
}

void ImageCompressor::sendCompressedImageNow(MCE,
    shared_ptr<HTTPRequest> httpRequest
) {
    REQUIRE_API_THREAD();

    flush(mce);

    compressedImage_(httpRequest);

    compressedImageUpdated_ = false;
    pump_(mce);
}

void ImageCompressor::sendCompressedImageWait(MCE,
    shared_ptr<HTTPRequest> httpRequest
) {
    REQUIRE_API_THREAD();

    flush(mce);

    if(compressedImageUpdated_) {
        sendCompressedImageNow(mce, httpRequest);
    } else {
        shared_ptr<ImageCompressor> self = shared_from_this();
        waitTag_ = postDelayedTask(sendTimeout_, [self, httpRequest]() {
            REQUIRE_API_THREAD();
            self->sendCompressedImageNow(mce, httpRequest);
        });
    }
}

void ImageCompressor::stopFetching() {
    REQUIRE_API_THREAD();
    fetchingStopped_ = true;
}

void ImageCompressor::flush(MCE) {
    REQUIRE_API_THREAD();

    if(waitTag_) {
        waitTag_->expedite();
    }
}

void ImageCompressor::setIframeSignal(MCE, int signal) {
    REQUIRE_API_THREAD();
    REQUIRE(signal >= 0 && signal < IframeSignalCount);

    if(iframeSignal_ != signal) {
        iframeSignal_ = signal;
        updateNotify(mce);
    }
}

void ImageCompressor::setCursorSignal(MCE, int signal) {
    REQUIRE_API_THREAD();
    REQUIRE(signal >= 0 && signal < CursorSignalCount);

    if(cursorSignal_ != signal) {
        cursorSignal_ = signal;
        updateNotify(mce);
    }
}

void ImageCompressor::afterConstruct_(shared_ptr<ImageCompressor> self) {
    shared_ptr<TaskQueue> taskQueue = TaskQueue::getActiveQueue();
    compressorThread_ = thread([this, taskQueue]() {
        ActiveTaskQueueLock activeTaskQueueLock(taskQueue);

        unique_lock<mutex> lock(compressorMutex_);
        while(true) {
            if(compressorShutdownScheduled_) {
                return;
            } else if(compressorTaskScheduled_) {
                compressorTaskScheduled_ = false;
                function<void()> task = compressorTask_;
                compressorTask_ = []() {};

                lock.unlock();
                task();
                lock = unique_lock<mutex>(compressorMutex_);
            } else {
                compressorCv_.wait(lock);
            }
        }
    });
}

tuple<vector<uint8_t>, size_t, size_t> ImageCompressor::fetchImage_(MCE) {
    REQUIRE_API_THREAD();
    REQUIRE(!fetchingStopped_);

    vector<uint8_t> data;
    size_t width;
    size_t height;

    if(shared_ptr<ImageCompressorEventHandler> eventHandler = eventHandler_.lock()) {
        bool funcCalled = false;
        auto func = [&](
            const uint8_t* srcImage,
            size_t srcWidth,
            size_t srcHeight,
            size_t srcPitch
        ) {
            REQUIRE(!funcCalled);
            funcCalled = true;
            REQUIRE(srcWidth);
            REQUIRE(srcHeight);

            srcWidth = min(srcWidth, (size_t)16384);
            srcHeight = min(srcHeight, (size_t)16384);

            width = srcWidth;
            height = srcHeight;

            while((int)(width % (size_t)IframeSignalCount) != iframeSignal_) {
                ++width;
            }
            while((int)(height % (size_t)CursorSignalCount) != cursorSignal_) {
                ++height;
            }

            data.resize(4 * width * height, (uint8_t)255);

            const uint8_t* srcLine = srcImage;
            uint8_t* line = data.data();
            for(size_t y = 0; y < srcHeight; ++y) {
                memcpy(line, srcLine, 4 * srcWidth - 1);
                srcLine += 4 * srcPitch;
                line += 4 * width;
            }
        };
        eventHandler->onImageCompressorFetchImage(func);
        REQUIRE(funcCalled);
    } else {
        data.resize(4, (uint8_t)255);
        width = 1;
        height = 1;
    }

    return {move(data), width, height};
}

void ImageCompressor::pump_(MCE) {
    REQUIRE_API_THREAD();

    if(
        fetchingStopped_ ||
        compressionInProgress_ ||
        !imageUpdated_ ||
        compressedImageUpdated_
    ) {
        return;
    }

    compressionInProgress_ = true;
    imageUpdated_ = false;

    int quality = quality_;

    vector<uint8_t> imageData;
    size_t imageWidth;
    size_t imageHeight;
    tie(imageData, imageWidth, imageHeight) = fetchImage_(mce);

    shared_ptr<ImageCompressor> self = shared_from_this();
    shared_ptr<PNGCompressor> pngCompressor = pngCompressor_;
    function<void()> task = [
        self,
        pngCompressor,
        quality,
        imageData{move(imageData)},
        imageWidth,
        imageHeight
    ]() {
        CompressedImage compressedImage;
        if(quality == 101) {
            compressedImage =
                compressPNG_(imageData, imageWidth, imageHeight, pngCompressor);
        } else {
            compressedImage =
                compressJPEG_(imageData, imageWidth, imageHeight, quality);
        }

        postTask(self, &ImageCompressor::compressTaskDone_, mce, compressedImage);
    };

    {
        lock_guard<mutex> lock(compressorMutex_);
        REQUIRE(!compressorTaskScheduled_);
        compressorTask_ = task;
        compressorTaskScheduled_ = true;
    }
    compressorCv_.notify_one();
}

void ImageCompressor::compressTaskDone_(MCE, CompressedImage compressedImage) {
    REQUIRE_API_THREAD();
    REQUIRE(compressionInProgress_);

    compressionInProgress_ = false;
    compressedImageUpdated_ = true;
    compressedImage_ = compressedImage;

    flush(mce);
}

}
