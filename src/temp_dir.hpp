#pragma once

#include "common.hpp"

namespace browservice {

class TempDir {
SHARED_ONLY_CLASS(TempDir);
public:
    TempDir(CKey);
    ~TempDir();

    const PathStr& path();

private:
    PathStr path_;
};

}
