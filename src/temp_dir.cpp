#include "temp_dir.hpp"

#ifdef _WIN32
#include <fileapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace browservice {

TempDir::TempDir(CKey) {
#ifdef _WIN32
    const DWORD bufSize = MAX_PATH + 2;
    wchar_t baseBuf[bufSize];
    DWORD baseLen = GetTempPathW(bufSize, baseBuf);
    REQUIRE(baseLen > 0);
    wstring path(baseBuf, (size_t)baseLen);

    path += L"browservicetmp_";
    wstring charPalette = L"abcdefghijklmnopqrstuvABCDEFGHIJKLMNOPQRSTUV0123456789";
    for(int i = 0; i < 16; ++i) {
        wchar_t c = charPalette[uniform_int_distribution<size_t>(0, charPalette.size() - 1)(rng)];
        path.push_back(c);
    }

    REQUIRE(CreateDirectoryW(path.c_str(), nullptr));
#else
    char path[] = "/tmp/browservicetmp_XXXXXX";
    REQUIRE(mkdtemp(path) != nullptr);
#endif

    path_ = path;
}

TempDir::~TempDir() {
#ifdef _WIN32
    if(!RemoveDirectoryW(path_.c_str())) {
        WARNING_LOG("Deleting temporary directory ", path_, " failed");
    }
#else
    if(rmdir(path_.c_str())) {
        WARNING_LOG("Deleting temporary directory ", path_, " failed");
    }
#endif
}

const PathStr& TempDir::path() {
    return path_;
}

}
