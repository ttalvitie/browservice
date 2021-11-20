#pragma once

#include "common.hpp"

namespace browservice {

class TempDir {
SHARED_ONLY_CLASS(TempDir);
public:
#ifdef _WIN32
    typedef wstring PathStr;
#else
    typedef string PathStr;
#endif

    TempDir(CKey);
    ~TempDir();

    const PathStr& path();

private:
    PathStr path_;
};

}
