#include "image_slice.hpp"

class HTTPRequest;
class Timeout;

// Image compressor service for a single browser session. The image pipeline is
// run asynchronously: raw images are fed in through updateImage, and compressed
// images are written to HTTPRequest objects supplied through
// sendCompressedImage*. At most one image is compressed at a time in a separate
// thread. At most one HTTP request is kept open at a time; the previous
// requests are responded to upon each sendImage* call.
class ImageCompressor {
SHARED_ONLY_CLASS(ImageCompressor);
public:
    ImageCompressor(CKey, int64_t sendTimeoutMs);

    void updateImage(ImageSlice image);

    // Send the most recent compressed image immediately
    void sendCompressedImageNow(shared_ptr<HTTPRequest> httpRequest);

    // Send the image once a new compressed image is available or the timeout
    // sendTimeoutMs (given in constructor) is reached
    void sendCompressedImageWait(shared_ptr<HTTPRequest> httpRequest);

private:
    shared_ptr<Timeout> sendTimeout_;
    function<void(shared_ptr<HTTPRequest>)> compressedImage_;
};
