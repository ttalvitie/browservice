#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class PNGCompressor {
public:
    PNGCompressor(size_t threadCount);
    ~PNGCompressor();

    // Compress given image into PNG. The image data should be in a format where
    // for all 0 <= y < height and 0 <= x < width, image[4 * (y * pitch + x) + c]
    // is the value for color blue, green and red for c = 0, 1, 2, respectively.
    // The resulting compressed PNG data can be obtained by concatenating the
    // returned chunks.
    // 
    // This function is not safe to call from multiple threads at the same time.
    std::vector<std::vector<uint8_t>> compress(
        const uint8_t* image,
        size_t width,
        size_t height,
        size_t pitch
    );

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
