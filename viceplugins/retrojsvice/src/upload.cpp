#include "upload.hpp"

#include <sys/stat.h>
#include <unistd.h>

namespace retrojsvice {

TempDir::TempDir(CKey) {
    char path[] = "/tmp/retrojsvicetmp_XXXXXX";
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

UploadStorage::UploadStorage(CKey) {
    tempDir_ = TempDir::create();
}

namespace {

string sanitizeName(string src) {
    string ret;
    for(char c : src) {
        if(c != '/' && c != '\0') {
            ret.push_back(c);
        }
    }
    if(ret.size() > (size_t)200) {
        ret.resize((size_t)200);
    }
    if(ret == "." || ret == "..") {
        ret += "_file.bin";
    }
    if(ret.empty()) {
        ret = "file.bin";
    }
    return ret;
}

}

shared_ptr<FileUpload> UploadStorage::upload(
    string name,
    istream& dataStream
) {
    name = sanitizeName(move(name));
    INFO_LOG("UPLOAD ", name);
    // TODO: implement
    return {};
}

}
