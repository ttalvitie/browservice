#include "jpeg.hpp"

#include "jpeg.hpp"

#include <cstdio>
#include <iostream>
#include <vector>

#include <jpeglib.h>

static void check(
    bool condVal,
    const char* condStr,
    const char* condFile,
    int condLine
) {
    if(!condVal) {
        std::cerr << "FATAL ERROR " << condFile << ":" << condLine << ": ";
        std::cerr << "Condition '" << condStr << "' does not hold\n";
        abort();
    }
}

#define CHECK(condition) check((condition), #condition, __FILE__, __LINE__)

JPEGData compressJPEG(
    const uint8_t* image,
    size_t width,
    size_t height,
    size_t pitch,
    int quality
) {
    CHECK(width > 0 && height > 0);
    CHECK(quality >= 1 && quality <= 100);

    struct jpeg_compress_struct jpegCtx;

    struct jpeg_error_mgr jpegErrorManager;
    jpegCtx.err = jpeg_std_error(&jpegErrorManager);

    jpeg_create_compress(&jpegCtx);

    uint8_t* outputBuf = nullptr;
    unsigned long outputLength = 0;
    jpeg_mem_dest(&jpegCtx, &outputBuf, &outputLength);

    jpegCtx.image_width = width;
    jpegCtx.image_height = height;
    jpegCtx.input_components = 3;
    jpegCtx.in_color_space = JCS_RGB;

    jpeg_set_defaults(&jpegCtx);
    jpeg_set_quality(&jpegCtx, quality, true);
    if(quality <= 90) {
        jpegCtx.dct_method = JDCT_IFAST;
    }

    jpeg_start_compress(&jpegCtx, true);

    std::vector<uint8_t> row(3 * width);
    JSAMPROW rowPointer[1];
    rowPointer[0] = row.data();

    while(jpegCtx.next_scanline < height) {
        const uint8_t* src = image + 4 * pitch * jpegCtx.next_scanline;
        uint8_t* dest = row.data();
        for(size_t x = 0; x < width; ++x) {
            *(dest + 0) = *(src + 2);
            *(dest + 1) = *(src + 1);
            *(dest + 2) = *(src + 0);
            src += 4;
            dest += 3;
        }
        (void)jpeg_write_scanlines(&jpegCtx, rowPointer, 1);
    }

    jpeg_finish_compress(&jpegCtx);

    JPEGData jpegData;
    jpegData.data.reset(outputBuf);
    jpegData.length = outputLength;

    jpeg_destroy_compress(&jpegCtx);

    return jpegData;
}
