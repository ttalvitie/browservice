#include "upload.hpp"

#include <Poco/Crypto/DigestEngine.h>

#include <sys/stat.h>
#include <sys/types.h>
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

void destroyFile(const string& dirPath, const string& filePath) {
    if(unlink(filePath.c_str())) {
        WARNING_LOG("Unlinking temporary file ", filePath, " failed");
    }
    if(rmdir(dirPath.c_str())) {
        WARNING_LOG("Deleting temporary directory ", dirPath, " failed");
    }
}

}

class FileUpload::Impl {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(
        CKey,
        shared_ptr<UploadStorage> storage,
        string name,
        string dirPath,
        string filePath,
        string hash
    ) {
        storage_ = storage;
        name_ = move(name);
        dirPath_ = move(dirPath);
        filePath_ = move(filePath);
        hash_ = move(hash);
    }

    // The shared_ptr must expire when storage_->mutex_ is locked.
    ~Impl() {
        pair<string, string> key(name_, hash_);
        REQUIRE(storage_->files_.erase(key));

        destroyFile(dirPath_, filePath_);
    }

private:
    shared_ptr<UploadStorage> storage_;
    string name_;
    string dirPath_;
    string filePath_;
    string hash_;

    friend class FileUpload;
};

FileUpload::FileUpload(CKey, shared_ptr<Impl> impl) {
    impl_ = impl;
}

FileUpload::~FileUpload() {
    shared_ptr<UploadStorage> storage = impl_->storage_;
    lock_guard<mutex> lock(storage->mutex_);
    impl_.reset();
}

const string& FileUpload::path() {
    return impl_->filePath_;
}

UploadStorage::UploadStorage(CKey) {
    tempDir_ = TempDir::create();
    nextIdx_.store((uint64_t)1, memory_order_relaxed);
}

UploadStorage::~UploadStorage() {
    REQUIRE(files_.empty());
}

shared_ptr<FileUpload> UploadStorage::upload(
    string name,
    istream& dataStream
) {
    name = sanitizeName(move(name));

    uint64_t idx = nextIdx_.fetch_add(1, memory_order_relaxed);

    string dirPath = tempDir_->path() + "/" + toString(idx);
    string filePath = dirPath + "/" + name;

    REQUIRE(mkdir(dirPath.c_str(), 0777) == 0);
    ofstream fp;
    fp.open(filePath);
    REQUIRE(fp.good());

    Poco::Crypto::DigestEngine hasher("SHA256");

    bool ok = true;
    try {
        const size_t BufSize = 1 << 16;
        char buf[BufSize];
        while(!dataStream.eof()) {
            dataStream.read(buf, BufSize);
            if(
                dataStream.bad() ||
                (dataStream.fail() != dataStream.eof())
            ) {
                WARNING_LOG("Reading file upload stream failed");
                ok = false;
                break;
            }

            size_t readCount = dataStream.gcount();
            REQUIRE(readCount >= 0 && readCount <= BufSize);

            hasher.update(buf, readCount);

            fp.write(buf, readCount);
            REQUIRE(fp.good());
        }
    } catch(...) {
        WARNING_LOG("Reading file upload stream failed with exception");
        ok = false;
    }

    fp.close();
    REQUIRE(fp.good());

    if(!ok) {
        destroyFile(dirPath, filePath);
        shared_ptr<FileUpload> empty;
        return empty;
    }

    string hash = hasher.digestToHex(hasher.digest());
    pair<string, string> key(name, hash);

    shared_ptr<FileUpload::Impl> impl;
    {
        lock_guard<mutex> lock(mutex_);
        auto it = files_.find(key);
        if(it == files_.end()) {
            impl = FileUpload::Impl::create(
                shared_from_this(),
                move(name),
                move(dirPath),
                move(filePath),
                move(hash)
            );
            REQUIRE(files_.emplace(key, impl).second);
        } else {
            impl = it->second.lock();
            REQUIRE(impl);
            destroyFile(dirPath, filePath);
        }
    }
    return FileUpload::create(impl);
}

}
