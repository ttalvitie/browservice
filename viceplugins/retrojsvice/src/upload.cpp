#include "upload.hpp"

#include <Poco/Crypto/DigestEngine.h>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace retrojsvice {

TempDir::TempDir(CKey) {
/*
    char path[] = "/tmp/retrojsvicetmp_XXXXXX";
    REQUIRE(mkdtemp(path) != nullptr);
    path_ = path;
*/
}

TempDir::~TempDir() {
/*
    if(rmdir(path_.c_str())) {
        WARNING_LOG("Deleting temporary directory ", path_, " failed");
    }
*/
}

const string& TempDir::path() {
    return path_;
}

namespace {

void unlinkFile(const string& path) {
/*
    if(unlink(path.c_str())) {
        WARNING_LOG("Unlinking temporary file ", path, " failed");
    }
*/
}

}

class FileUpload::Impl {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(
        CKey,
        shared_ptr<UploadStorage> storage,
        string name,
        string path,
        string hash
    ) {
        storage_ = storage;
        name_ = move(name);
        path_ = move(path);
        hash_ = move(hash);
    }

    // The shared_ptr must expire when storage_->mutex_ is locked.
    ~Impl() {
        REQUIRE(storage_->files_.erase(hash_));
        unlinkFile(path_);
    }

private:
    shared_ptr<UploadStorage> storage_;
    string name_;
    string path_;
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
    return impl_->path_;
}

const string& FileUpload::name() {
    return impl_->name_;
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
    uint64_t idx = nextIdx_.fetch_add(1, memory_order_relaxed);

    string path = tempDir_->path() + "/" + toString(idx);

    ofstream fp;
    fp.open(path);
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
        unlinkFile(path);
        shared_ptr<FileUpload> empty;
        return empty;
    }

    string hash = hasher.digestToHex(hasher.digest());

    shared_ptr<FileUpload> ret;
    {
        lock_guard<mutex> lock(mutex_);
        auto it = files_.find(hash);
        shared_ptr<FileUpload::Impl> impl;
        if(it == files_.end()) {
            impl = FileUpload::Impl::create(
                shared_from_this(),
                move(name),
                move(path),
                hash
            );
            REQUIRE(files_.emplace(hash, impl).second);
        } else {
            impl = it->second.lock();
            REQUIRE(impl);
            unlinkFile(path);
        }
        ret = FileUpload::create(impl);
    }
    return ret;
}

string extractUploadFilename(string src) {
    string ret;
    for(char c : src) {
        if(c == '/' || c == '\\') {
            ret = "";
        } else if(c != '\0') {
            ret.push_back(c);
        }
    }
    return ret;
}

}
