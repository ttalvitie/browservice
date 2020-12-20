#pragma once

#include "common.hpp"

class PNGCompressor;

namespace retrojsvice {

class ImageCompressorEventHandler {
public:
    // Exceptionally called by ImageCompressor directly (instead of the task
    // queue) to fetch the latest image frame. The handler must call func
    // exactly once with the image specs before returning. The image is
    // specified using the argument set (image, width, height, pitch), where
    // width > 0 and height > 0. For all 0 <= y < height and 0 <= x < width,
    // image[4 * (y * pitch + x) + c] is the value for color blue, green and red
    // for c = 0, 1, 2, respectively. The callback func will not retain the
    // image pointer; it will copy the data before returning.
    virtual void onFetchImage(
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;
};

class DelayedTaskTag;
class HTTPRequest;

// Image compressor service for a single browser session. The image pipeline is
// run asynchronously: when an updated image is available, the service is
// notified by calling updateNotify(); when it is ready to begin compressing it,
// it uses the onFetchImage event handler to fetch the most recent image.
// At most one image is being compressed at a time in a separate background
// thread. At most one HTTP request is kept waiting for a new image to complete
// at a time; the previous requests are responded to upon each
// sendCompressedImage* call.
class ImageCompressor : public enable_shared_from_this<ImageCompressor> {
SHARED_ONLY_CLASS(ImageCompressor);
public:
    ImageCompressor(CKey,
        weak_ptr<ImageCompressorEventHandler> eventHandler,
        steady_clock::duration sendTimeout,
        bool allowPNG,
        int quality
    );
    ~ImageCompressor();

    void setQuality(int quality);

    void updateNotify();

    // Send the most recent compressed image immediately
    void sendCompressedImageNow(shared_ptr<HTTPRequest> httpRequest);

    // Send the image once a new compressed image is available or the timeout
    // sendTimeout (given in constructor) is reached
    void sendCompressedImageWait(shared_ptr<HTTPRequest> httpRequest);

    // Flush possible pending sendCompressedImageWait request with the latest
    // image available immediately
    void flush();

private:
    void afterConstruct_(shared_ptr<ImageCompressor> self);

    typedef function<void(shared_ptr<HTTPRequest>)> CompressedImage;

    tuple<vector<uint8_t>, size_t, size_t> fetchImage_();

    void pump_();
    void compressTaskDone_(CompressedImage compressedImage);

    weak_ptr<ImageCompressorEventHandler> eventHandler_;
    steady_clock::duration sendTimeout_;
    bool allowPNG_;
    int quality_;

    shared_ptr<PNGCompressor> pngCompressor_;

    thread compressorThread_;
    mutex compressorMutex_;
    condition_variable compressorCv_;
    bool compressorShutdownScheduled_;
    bool compressorTaskScheduled_;
    function<void()> compressorTask_;

    shared_ptr<DelayedTaskTag> waitTag_;
    CompressedImage compressedImage_;

    bool imageUpdated_;
    bool compressedImageUpdated_;
    bool compressionInProgress_;
};

}
