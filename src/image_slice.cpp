#include "image_slice.hpp"

namespace browservice {

ImageSlice ImageSlice::createImage(int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    ImageSlice slice = createImage(width, height, 0);
    slice.fill(0, width, 0, height, r, g, b);
    return slice;
}

ImageSlice ImageSlice::createImage(int width, int height, uint8_t rgb) {
    REQUIRE(width >= 0 && height >= 0);
    const int Limit = INT_MAX / 9;
    REQUIRE(width < Limit && height < Limit);
    if(height > 0) {
        REQUIRE(width < Limit / height);
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

ImageSlice ImageSlice::createImageFromStrings(
    const vector<string>& rows,
    const map<char, array<uint8_t, 3>>& colors
) {
    if(rows.empty()) {
        return createImage(0, 0);
    }

    int height = (int)rows.size();
    int width = (int)rows[0].size();
    for(int y = 1; y < height; ++y) {
        REQUIRE(rows[y].size() == (size_t)width);
    }

    ImageSlice ret = createImage(width, height, 0);

    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            char colorChar = rows[y][x];
            auto it = colors.find(colorChar);
            REQUIRE(it != colors.end());
            ret.setPixel(x, y, it->second[0], it->second[1], it->second[2]);
        }
    }

    return ret;
}

}
