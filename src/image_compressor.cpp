#include "image_compressor.hpp"

#include "globals.hpp"
#include "http.hpp"
#include "jpeg.hpp"
#include "png.hpp"
#include "quality.hpp"
#include "timeout.hpp"

#include "include/cef_thread.h"
#include "include/wrapper/cef_closure_task.h"

namespace {

void serveWhiteJPEGPixel(shared_ptr<HTTPRequest> request) {
    CEF_REQUIRE_UI_THREAD();

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

ImageCompressor::ImageCompressor(CKey, int64_t sendTimeoutMs) {
    CEF_REQUIRE_UI_THREAD();

    sendTimeout_ = Timeout::create(sendTimeoutMs);
    compressorThread_ = CefThread::CreateThread("Image compressor");

    quality_ = globals->defaultQuality;

    int pngThreadCount = (int)thread::hardware_concurrency();
    pngThreadCount = min(pngThreadCount, 4);
    pngThreadCount = max(pngThreadCount, 1);

    pngCompressor_ = make_shared<PNGCompressor>(pngThreadCount);

    // Prior to compressing the first image, our image is a white pixel
    image_ = ImageSlice::createImage(1, 1);
    compressedImage_ = [](shared_ptr<HTTPRequest> request) {
        serveWhiteJPEGPixel(request);
    };

    imageUpdated_ = false;
    compressedImageUpdated_ = false;
    compressionInProgress_ = false;
}

ImageCompressor::~ImageCompressor() {}

void ImageCompressor::setQuality(int quality) {
    CHECK(quality >= MinQuality && quality <= MaxQuality);
    if(quality != quality_) {
        quality_ = quality;
        imageUpdated_ = true;
        pump_();
    }
}

void ImageCompressor::updateImage(ImageSlice image) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(!image.isEmpty());

    image_ = image;
    imageUpdated_ = true;
    pump_();
}

void ImageCompressor::sendCompressedImageNow(shared_ptr<HTTPRequest> httpRequest) {
    CEF_REQUIRE_UI_THREAD();

    sendTimeout_->clear(true);

    compressedImage_(httpRequest);

    compressedImageUpdated_ = false;
    pump_();
}

void ImageCompressor::sendCompressedImageWait(shared_ptr<HTTPRequest> httpRequest) {
    CEF_REQUIRE_UI_THREAD();

    sendTimeout_->clear(true);

    if(compressedImageUpdated_) {
        sendCompressedImageNow(httpRequest);
    } else {
        shared_ptr<ImageCompressor> self = shared_from_this();
        sendTimeout_->set([self, httpRequest]() {
            CEF_REQUIRE_UI_THREAD();
            self->sendCompressedImageNow(httpRequest);
        });
    }
}

void ImageCompressor::flush() {
    CEF_REQUIRE_UI_THREAD();
    sendTimeout_->clear(true);
}

ImageCompressor::CompressedImage ImageCompressor::compressPNG_(
    ImageSlice image,
    shared_ptr<PNGCompressor> pngCompressor
) {
    shared_ptr<vector<vector<uint8_t>>> png =
        make_shared<vector<vector<uint8_t>>>(
            pngCompressor->compress(
                image.buf(),
                image.width(),
                image.height(),
                image.pitch()
            )
        );

    uint64_t length = 0;
    for(const vector<uint8_t>& chunk : *png) {
        length += chunk.size();
    }

    return [png, length](shared_ptr<HTTPRequest> request) {
        CEF_REQUIRE_UI_THREAD();

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

ImageCompressor::CompressedImage ImageCompressor::compressJPEG_(
    ImageSlice image,
    int quality
) {
    CHECK(quality > 0 && quality <= 100);

    shared_ptr<JPEGData> jpeg = make_shared<JPEGData>(compressJPEG(
        image.buf(),
        image.width(),
        image.height(),
        image.pitch(),
        quality
    ));
    return [jpeg](shared_ptr<HTTPRequest> request) {
        CEF_REQUIRE_UI_THREAD();

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

void ImageCompressor::pump_() {
    if(compressionInProgress_ || !imageUpdated_ || compressedImageUpdated_) {
        return;
    }

    CHECK(!image_.isEmpty());

    compressionInProgress_ = true;
    imageUpdated_ = false;

    int quality = quality_;
    ImageSlice imageCopy = image_.clone();
    shared_ptr<ImageCompressor> self = shared_from_this();
    shared_ptr<PNGCompressor> pngCompressor = pngCompressor_;
    function<void()> compressTask = [quality, imageCopy, self, pngCompressor]() mutable {
        CompressedImage compressedImage;
        if(quality == MaxQuality) {
            compressedImage = compressPNG_(imageCopy, pngCompressor);
        } else {
            compressedImage = compressJPEG_(imageCopy, quality);
        }

        postTask(self, &ImageCompressor::compressTaskDone_, compressedImage);
    };

    void (*call)(function<void()>) = [](function<void()> func) {
        func();
    };
    compressorThread_->GetTaskRunner()->PostTask(
        CefCreateClosureTask(base::Bind(call, compressTask))
    );
}

void ImageCompressor::compressTaskDone_(CompressedImage compressedImage) {
    CEF_REQUIRE_UI_THREAD();
    CHECK(compressionInProgress_);

    compressionInProgress_ = false;
    compressedImageUpdated_ = true;
    compressedImage_ = compressedImage;

    sendTimeout_->clear(true);
}
