#pragma once

#include "common.hpp"

// Reference to a rectangular part of a shared image buffer
class ImageSlice {
public:
    // Create an empty image slice
    ImageSlice()
        : buf_(nullptr),
          width_(0),
          height_(0),
          pitch_(0),
          globalX_(0),
          globalY_(0)
    {}

    // Create a new independent width x height image buffer with background
    // color (r, g, b)
    static ImageSlice createImage(int width, int height, uint8_t r, uint8_t g, uint8_t b);
    static ImageSlice createImage(int width, int height, uint8_t rgb = 255);

    // Returns pointer buf such that for all 0 <= y < height() and
    // 0 <= x < width(), buf[4 * (y * pitch() + x) + c] is the value in pixel
    // (x, y) for color blue, green and red for c = 0, 1, 2, respectively
    uint8_t* buf() { return buf_; }

    int width() { return width_; }
    int height() { return height_; };

    int pitch() { return pitch_; }

    // Coordinates of the upper left corner of this slice in the original shared
    // image buffer
    int globalX() { return globalX_; }
    int globalY() { return globalY_; }

    bool containsGlobalPoint(int gx, int gy) {
        int x = gx - globalX_;
        int y = gy - globalY_;
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    // Returns true if the slice is empty, i.e. contains 0 pixels
    bool isEmpty() {
        return width_ == 0 || height_ == 0;
    }

    // Get pointer to given pixel. Does no bounds checking; can be used with
    // x = width to obtain past-the-end-of-line pointer
    uint8_t* getPixelPtr(int x, int y) {
        return &buf_[4 * (y * pitch_ + x)];
    }

    // Set pixel in slice to given RGB value. If the point (x, y) is outside
    // the slice, does nothing
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if(x >= 0 && x < width_ && y >= 0 && y < height_) {
            uint8_t* pos = getPixelPtr(x, y);
            *(pos + 2) = r;
            *(pos + 1) = g;
            *(pos + 0) = b;
        }
    }
    void setPixel(int x, int y, uint8_t rgb) {
        setPixel(x, y, rgb, rgb, rgb);
    }

    // Get image slice for subrectangle [startX, endX) x [startY, endY] of this
    // slice. Clamps the given coordinates such that they are inside the slice
    // and in correct order.
    ImageSlice subRect(int startX, int endX, int startY, int endY) {
        clampBoundX_(startX);
        clampBoundX_(endX);
        clampBoundY_(startY);
        clampBoundY_(endY);
        endX = max(endX, startX);
        endY = max(endY, startY);

        ImageSlice ret = *this;
        ret.width_ = endX - startX;
        ret.height_ = endY - startY;
        ret.buf_ += 4 * (startY * ret.pitch_ + startX);
        ret.globalX_ += startX;
        ret.globalY_ += startY;
        return ret;
    }

    // Return the image slice split into two parts by X or Y coordinate. Clamps
    // the given coordinate into the valid range [0, width()] or [0, height()].
    pair<ImageSlice, ImageSlice> splitX(int x) {
        return {
            subRect(0, x, 0, height_),
            subRect(x, width_, 0, height_)
        };
    }
    pair<ImageSlice, ImageSlice> splitY(int y) {
        return {
            subRect(0, width_, 0, y),
            subRect(0, width_, y, height_)
        };
    }

    // Fill subrectangle [startX, endX) x [startY, endY] with given color
    /// (r, g, b). Clamps the given coordinates such that they are inside the
    // slice and in correct order.
    void fill(
        int startX, int endX, int startY, int endY,
        uint8_t r, uint8_t g, uint8_t b
    ) {
        clampBoundX_(startX);
        clampBoundX_(endX);
        clampBoundY_(startY);
        clampBoundY_(endY);
        endX = max(endX, startX);
        endY = max(endY, startY);

        for(int y = startY; y < endY; ++y) {
            uint8_t* pos = getPixelPtr(startX, y);
            for(int x = startX; x < endX; ++x) {
                *(pos + 2) = r;
                *(pos + 1) = g;
                *(pos + 0) = b;
                pos += 4;
            }
        }
    }
    void fill(int startX, int endX, int startY, int endY, uint8_t rgb) {
        clampBoundX_(startX);
        clampBoundX_(endX);
        clampBoundY_(startY);
        clampBoundY_(endY);
        endX = max(endX, startX);
        endY = max(endY, startY);

        for(int y = startY; y < endY; ++y) {
            uint8_t* startPos = getPixelPtr(startX, y);
            uint8_t* endPos = getPixelPtr(endX, y);
            std::fill(startPos, endPos, rgb);
        }
    }

    // Create an a copy of the contents of the slice as a new independent image
    // buffer (globalX() and globalY() are reset to zero). It is safe to move
    // the resulting slice to another thread and modify it there independently
    // of the modifications to the current slice in the current thread.
    ImageSlice clone() {
        ImageSlice ret;

        ret.globalBuf_.reset(new vector<uint8_t>());
        ret.globalBuf_->reserve(4 * width_ * height_);
        for(int y = 0; y < height_; ++y) {
            ret.globalBuf_->insert(
                ret.globalBuf_->end(), getPixelPtr(0, y), getPixelPtr(width_, y)
            );
        }

        ret.buf_ = ret.globalBuf_->data();
        ret.width_ = width_;
        ret.height_ = height_;
        ret.pitch_ = width_;
        ret.globalX_ = 0;
        ret.globalY_ = 0;
        return ret;
    }


private:
    void clampBoundX_(int& x) {
        x = max(min(x, width_), 0);
    }
    void clampBoundY_(int& y) {
        y = max(min(y, height_), 0);
    }

    shared_ptr<vector<uint8_t>> globalBuf_;

    uint8_t* buf_;

    int width_;
    int height_;

    int pitch_;

    int globalX_;
    int globalY_;
};
