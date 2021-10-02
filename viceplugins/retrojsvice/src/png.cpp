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

#include "png.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <winsock.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

#include <zlib.h>

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

namespace {

std::array<uint32_t, 256> computeCRCTable() {
    std::array<uint32_t, 256> ret;
    for(int i = 0; i < 256; ++i) {
        uint32_t c = (uint32_t)i;
        for(int k = 0; k < 8; ++k) {
            if(c & 1) {
                c = UINT32_C(0xedb88320) ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        ret[i] = c;
    }
    return ret;
}
const std::array<uint32_t, 256> crcTable = computeCRCTable();

class ChunkWriter {
public:
    ChunkWriter(std::vector<uint8_t>& buf, const char type[4])
        : buf_(buf),
          startPos_(buf.size()),
          crc32_(UINT32_C(0xffffffff))
    {
        buf_.resize(startPos_ + 4);
        write(type, 4);
    }
    void registerWrite(size_t startPos) {
        for(size_t i = startPos; i < buf_.size(); ++i) {
            crc32_ = crcTable[(crc32_ ^ buf_[i]) & 0xff] ^ (crc32_ >> 8);
        }
    }
    void write(const void* data, size_t size) {
        size_t pos = buf_.size();
        buf_.resize(pos + size);
        memcpy(buf_.data() + pos, data, size);
        
        registerWrite(pos);
    }
    void writeU8(uint8_t val) {
        write(&val, 1);
    }
    void writeU32(uint32_t val) {
        uint32_t valNB = htonl(val);
        write(&valNB, 4);
    }
    void finish() {
        uint32_t dataLength = (uint32_t)(buf_.size() - startPos_ - 8);
        uint32_t dataLengthNB = htonl(dataLength);
        memcpy(buf_.data() + startPos_, &dataLengthNB, 4);

        crc32_ = ~crc32_;
        uint32_t crc32NB = htonl(crc32_);
        size_t pos = buf_.size();
        buf_.resize(pos + 4);
        memcpy(buf_.data() + pos, &crc32NB, 4);
    }

private:
    std::vector<uint8_t>& buf_;
    size_t startPos_;
    uint32_t crc32_;
};

struct Result {
    size_t uncompressedBytes;
    uint32_t adler32;
    std::vector<uint8_t> chunk;
};

struct JobData {
    const uint8_t* image;
    size_t width;
    size_t pitch;
    size_t startY;
    size_t endY;
    bool endStream;
};

struct Job {
    bool shutdown;
    std::promise<Result> resultPromise;
    std::future<std::unique_ptr<Job>> nextJobFuture;
    JobData data;
};

struct Worker {
    std::thread thread;
    std::promise<std::unique_ptr<Job>> jobPromise;
};

int paeth(int leftVal, int upVal, int upLeftVal) {
    int p = leftVal + upVal - upLeftVal;
    int pLeftVal = std::abs(p - leftVal);
    int pUpVal = std::abs(p - upVal);
    int pUpLeftVal = std::abs(p - upLeftVal);
    if(pLeftVal <= pUpVal && pLeftVal <= pUpLeftVal) {
        return leftVal;
    } else if(pUpVal <= pUpLeftVal) {
        return upVal;
    } else {
        return upLeftVal;
    }
}

Result runJob(JobData jobData) {
    const uint8_t* image = jobData.image;
    size_t width = jobData.width;
    size_t pitch = jobData.pitch;
    size_t startY = jobData.startY;
    size_t endY = jobData.endY;
    bool endStream = jobData.endStream;

    CHECK(startY < endY);

    size_t heightOut = endY - startY;
    size_t uncompressedBytes = heightOut * (1 + 3 * width);

    std::vector<uint8_t> rawData;
    rawData.reserve(uncompressedBytes);

    for(size_t y = startY; y < endY; ++y) {
        const uint8_t* imagePos = &image[4 * y * pitch];
        if(y == 0) {
            // First line is filtered by left subtraction
            rawData.push_back(1);
            int leftVal[3] = {0, 0, 0};
            for(size_t x = 0; x < width; ++x) {
                for(size_t c = 0; c < 3; ++c) {
                    int val = *(imagePos + 2 - c);
                    rawData.push_back((uint8_t)(val - leftVal[c]));
                    leftVal[c] = val;
                }
                imagePos += 4;
            }
        } else {
            // The rest of the lines are filtered using Paeth
            const uint8_t* upImagePos = imagePos - 4 * pitch;
            rawData.push_back(4);
            int leftVal[3] = {0, 0, 0};
            int upLeftVal[3] = {0, 0, 0};
            for(size_t x = 0; x < width; ++x) {
                for(size_t c = 0; c < 3; ++c) {
                    int val = *(imagePos + 2 - c);
                    int upVal = *(upImagePos + 2 - c);
                    int pred = paeth(leftVal[c], upVal, upLeftVal[c]);
                    rawData.push_back((uint8_t)(val - pred));
                    leftVal[c] = val;
                    upLeftVal[c] = upVal;
                }
                imagePos += 4;
                upImagePos += 4;
            }
        }
    }

    CHECK(rawData.size() == uncompressedBytes);

    z_stream zStream;
    zStream.zalloc = nullptr;
    zStream.zfree = nullptr;
    zStream.opaque = nullptr;
    CHECK(deflateInit2(&zStream, 1, Z_DEFLATED, 15, 8, Z_RLE) == Z_OK);

    zStream.avail_in = (unsigned int)uncompressedBytes;
    zStream.next_in = rawData.data();

    std::vector<uint8_t> chunk;
    ChunkWriter writer(chunk, "IDAT");
    size_t zStreamStart = chunk.size();

    bool headerRead = false;
    bool bufErrorSeen = false;
    while(true) {
        size_t blockSize = headerRead ? 8192 : 64;

        size_t pos = chunk.size();
        chunk.resize(pos + blockSize);

        zStream.avail_out = (unsigned int)blockSize;
        zStream.next_out = chunk.data() + pos;

        int flush;
        if(bufErrorSeen) {
            flush = endStream ? Z_FINISH : Z_SYNC_FLUSH;;
        } else {
            flush = Z_NO_FLUSH;
        }
        int res = deflate(&zStream, flush);
        CHECK(res == Z_OK || res == Z_STREAM_END || res == Z_BUF_ERROR);

        chunk.resize(chunk.size() - zStream.avail_out);

        if(!headerRead && chunk.size() >= zStreamStart + 2) {
            // Check ZLIB header
            CHECK((chunk[zStreamStart] & 0xf) == 8); // deflate compression
            CHECK((chunk[zStreamStart] >> 4) == 7); // 32K window size
            CHECK(!(chunk[zStreamStart + 1] & 32)); // no preset dictionary
            chunk.erase(chunk.begin() + zStreamStart, chunk.begin() + zStreamStart + 2);
            headerRead = true;
        }

        if(bufErrorSeen) {
            if(res == Z_STREAM_END || res == Z_BUF_ERROR) {
                break;
            }
        } else {
            if(res == Z_BUF_ERROR) {
                bufErrorSeen = true;
            }
        }
    }
    CHECK(headerRead);

    uint32_t adler32 = zStream.adler;
    
    int res = deflateEnd(&zStream);
    CHECK(res == Z_OK || res == Z_DATA_ERROR);

    // Remove Adler32 value from the end of data
    if(endStream) {
        CHECK(chunk.size() >= 4);
        chunk.resize(chunk.size() - 4);
    }

    writer.registerWrite(zStreamStart);
    writer.finish();

    return {uncompressedBytes, adler32, std::move(chunk)};
}

void workerThread(std::future<std::unique_ptr<Job>> jobFuture) {
    while(true) {
        std::unique_ptr<Job> job = jobFuture.get();
        if(job->shutdown) {
            break;
        }
        jobFuture = std::move(job->nextJobFuture);
        job->resultPromise.set_value(runJob(std::move(job->data)));
    }
}

}

class PNGCompressor::Impl {
public:
    Impl(size_t threadCount);
    ~Impl();

    std::vector<std::vector<uint8_t>> compress(
        const uint8_t* image,
        size_t width,
        size_t height,
        size_t pitch
    );

private:
    std::vector<Worker> workers_;
};

PNGCompressor::Impl::Impl(size_t threadCount) {
    CHECK(threadCount >= 1);
    for(size_t i = 1; i < threadCount; ++i) {
        std::promise<std::unique_ptr<Job>> jobPromise;
        std::future<std::unique_ptr<Job>> jobFuture = jobPromise.get_future();
        std::thread thread([jobFuture{std::move(jobFuture)}]() mutable {
            workerThread(std::move(jobFuture));
        });
        workers_.push_back({std::move(thread), std::move(jobPromise)});
    }
}
PNGCompressor::Impl::~Impl() {
    for(Worker& worker : workers_) {
        std::unique_ptr<Job> job = std::make_unique<Job>();
        job->shutdown = true;
        worker.jobPromise.set_value(std::move(job));
        worker.thread.join();
    }
}

std::vector<std::vector<uint8_t>> PNGCompressor::Impl::compress(
    const uint8_t* image,
    size_t width,
    size_t height,
    size_t pitch
) {
    CHECK(width > 0 && height > 0);

    size_t threadCount = std::min(workers_.size() + 1, height);

    std::vector<JobData> jobDatas(threadCount);
    for(size_t i = 0; i < threadCount; ++i) {
        JobData& jobData = jobDatas[i];
        jobData.image = image;
        jobData.width = width;
        jobData.pitch = pitch;
        jobData.startY = height * i / threadCount;
        jobData.endY = height * (i + 1) / threadCount;
        jobData.endStream = i + 1 == threadCount;
    }

    std::vector<std::future<Result>> resultFutures(threadCount - 1);
    for(size_t i = 1; i < threadCount; ++i) {
        std::promise<std::unique_ptr<Job>> nextJobPromise;
        std::unique_ptr<Job> job = std::make_unique<Job>();
        job->shutdown = false;
        resultFutures[i - 1] = job->resultPromise.get_future();
        job->nextJobFuture = nextJobPromise.get_future();
        job->data = std::move(jobDatas[i]);

        workers_[i - 1].jobPromise.set_value(std::move(job));
        workers_[i - 1].jobPromise = std::move(nextJobPromise);
    }

    std::vector<Result> results(threadCount);
    results[0] = runJob(std::move(jobDatas[0]));
    for(size_t i = 1; i < threadCount; ++i) {
        results[i] = resultFutures[i - 1].get();
    }

    std::vector<std::vector<uint8_t>> chunks;
    std::vector<uint8_t> headerData;

    // PNG signature
    headerData.push_back((uint8_t)137);
    headerData.push_back((uint8_t)80);
    headerData.push_back((uint8_t)78);
    headerData.push_back((uint8_t)71);
    headerData.push_back((uint8_t)13);
    headerData.push_back((uint8_t)10);
    headerData.push_back((uint8_t)26);
    headerData.push_back((uint8_t)10);

    {
        ChunkWriter writer(headerData, "IHDR");
        writer.writeU32((uint32_t)width);
        writer.writeU32((uint32_t)height);
        writer.writeU8(8); // bit depth 8
        writer.writeU8(2); // color type RGB
        writer.writeU8(0); // compression method standard
        writer.writeU8(0); // filter method standard
        writer.writeU8(0); // no interlace
        writer.finish();
    }
    {
        ChunkWriter writer(headerData, "IDAT");

        // ZLIB header
        writer.writeU8(8 | (7 << 4)); // compression method deflate, 32K window size
        writer.writeU8(1); // no preset dictionary, check bits 1

        writer.finish();
    }
    chunks.push_back(std::move(headerData));

    for(Result& result : results) {
        chunks.push_back(std::move(result.chunk));
    }

    std::vector<uint8_t> footerData;
    {
        ChunkWriter writer(footerData, "IDAT");

        // Combined adler32 value terminates the ZLIB stream
        uint32_t adler32 = 1;
        for(const Result& result : results) {
            adler32 = adler32_combine(adler32, result.adler32, (long)result.uncompressedBytes);
        }
        writer.writeU32(adler32);

        writer.finish();
    }
    {
        ChunkWriter writer(footerData, "IEND");
        writer.finish();
    }
    chunks.push_back(std::move(footerData));

    return chunks;
}

PNGCompressor::PNGCompressor(size_t threadCount)
    : impl_(new Impl(threadCount))
{}

PNGCompressor::~PNGCompressor() {}

std::vector<std::vector<uint8_t>> PNGCompressor::compress(
    const uint8_t* image,
    size_t width,
    size_t height,
    size_t pitch
) {
    return impl_->compress(image, width, height, pitch);
}
