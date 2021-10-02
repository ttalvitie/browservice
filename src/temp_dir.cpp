/*
#include "temp_dir.hpp"

#include <sys/stat.h>
#include <unistd.h>

namespace browservice {

TempDir::TempDir(CKey) {
    char path[] = "/tmp/browservicetmp_XXXXXX";
    REQUIRE(mkdtemp(path) != nullptr);
    path_ = path;
}

TempDir::~TempDir() {
    if(rmdir(path_.c_str())) {
        WARNING_LOG("Deleting temporary directory ", path_, " failed");
    }
}

const string& TempDir::path() {
    return path_;
}

}
*/
