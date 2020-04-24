#include "image_slice.hpp"

ImageSlice ImageSlice::createImage(int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    ImageSlice slice = createImage(width, height, 0);
    slice.fill(0, width, 0, height, r, g, b);
    return slice;
}

ImageSlice ImageSlice::createImage(int width, int height, uint8_t rgb) {
    CHECK(width >= 0 && height >= 0);
    const int Limit = INT_MAX / 9;
    CHECK(width < Limit && height < Limit);
    if(height > 0) {
        CHECK(width < Limit / height);
    }

    ImageSlice slice;
    slice.globalBuf_.reset(new vector<uint8_t>(4 * width * height, rgb));
    slice.buf_ = slice.globalBuf_->data();
    slice.width_ = width;
    slice.height_ = height;
    slice.pitch_ = width;
    slice.globalX_ = 0;
    slice.globalY_ = 0;
    return slice;
}
