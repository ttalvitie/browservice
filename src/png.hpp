/*
MIT License

Copyright (c) 2020 Topi Talvitie

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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
    // This function is not safe to call from multiple threads at the same time
    // for the same PNGCompressor object.
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
