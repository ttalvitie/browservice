#include "image_compressor.hpp"

#include "http.hpp"
#include "timeout.hpp"

ImageCompressor::ImageCompressor(CKey, int64_t sendTimeoutMs) {
    CEF_REQUIRE_UI_THREAD();

    sendTimeout_ = Timeout::create(sendTimeoutMs);
    compressedImage_ = [](shared_ptr<HTTPRequest> request) {
        // 1x1 white PNG
        vector<uint8_t> data = {
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0,
            0, 1, 0, 0, 0, 1, 8, 2, 0, 0, 0, 144, 119, 83, 222, 0, 0, 0, 12, 73,
            68, 65, 84, 8, 215, 99, 248, 255, 255, 63, 0, 5, 254, 2, 254, 220,
            204, 89, 231, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
        };
        uint64_t contentLength = data.size();
        request->sendResponse(
            200,
            "image/png",
            contentLength,
            [data{move(data)}](ostream& out) {
                out.write((const char*)data.data(), data.size());
            }
        );
    };
}

void ImageCompressor::updateImage(ImageSlice image) {
    CEF_REQUIRE_UI_THREAD();
    LOG(INFO) << "updateImage called, not implemented tho";
}

void ImageCompressor::sendCompressedImageNow(shared_ptr<HTTPRequest> httpRequest) {
    CEF_REQUIRE_UI_THREAD();
    compressedImage_(httpRequest);
}

void ImageCompressor::sendCompressedImageWait(shared_ptr<HTTPRequest> httpRequest) {
    CEF_REQUIRE_UI_THREAD();
    compressedImage_(httpRequest);
}
