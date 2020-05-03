#include "image_slice.hpp"

class HTTPRequest;
class Timeout;

class CefThread;

// Image compressor service for a single browser session. The image pipeline is
// run asynchronously: raw images are fed in through updateImage, and compressed
// images are written to HTTPRequest objects supplied through
// sendCompressedImage*. At most one image is compressed at a time in a separate
// thread. At most one HTTP request is kept open at a time; the previous
// requests are responded to upon each sendImage* call.
class ImageCompressor : public enable_shared_from_this<ImageCompressor> {
SHARED_ONLY_CLASS(ImageCompressor);
public:
    ImageCompressor(CKey, int64_t sendTimeoutMs);
    ~ImageCompressor();

    // The compressor may copy the image contents to be compressed from image
    // later than this call in CEF UI thread. Image must be nonempty.
    void updateImage(ImageSlice image);

    // Send the most recent compressed image immediately
    void sendCompressedImageNow(shared_ptr<HTTPRequest> httpRequest);

    // Send the image once a new compressed image is available or the timeout
    // sendTimeoutMs (given in constructor) is reached
    void sendCompressedImageWait(shared_ptr<HTTPRequest> httpRequest);

    // Flush possible pending sendCompressedImageWait request with the latest
    // image available immediately
    void flush();

private:
    typedef function<void(shared_ptr<HTTPRequest>)> CompressedImage;
    void pump_();
    void compressTaskDone_(CompressedImage compressedImage);

    shared_ptr<Timeout> sendTimeout_;
    CefRefPtr<CefThread> compressorThread_;

    ImageSlice image_;
    CompressedImage compressedImage_;

    bool imageUpdated_;
    bool compressedImageUpdated_;
    bool compressionInProgress_;
};
