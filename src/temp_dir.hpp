#pragma once

#include "common.hpp"

class TempDir {
SHARED_ONLY_CLASS(TempDir);
public:
    TempDir(CKey);
    ~TempDir();

    const string& path();

private:
    string path_;
};
