#include "upload.hpp"

#include <openssl/sha.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace retrojsvice {

TempDir::TempDir(CKey) {
#ifdef _WIN32
    const DWORD bufSize = MAX_PATH + 2;
    wchar_t baseBuf[bufSize];
    DWORD baseLen = GetTempPathW(bufSize, baseBuf);
    REQUIRE(baseLen > 0);
    wstring path(baseBuf, (size_t)baseLen);

    mt19937 rng(random_device{}());

    path += L"retrojsvicetmp_";
    wstring charPalette = L"abcdefghijklmnopqrstuvABCDEFGHIJKLMNOPQRSTUV0123456789";
    for(int i = 0; i < 16; ++i) {
        wchar_t c = charPalette[uniform_int_distribution<size_t>(0, charPalette.size() - 1)(rng)];
        path.push_back(c);
    }

    REQUIRE(CreateDirectoryW(path.c_str(), nullptr));
#else
    char path[] = "/tmp/retrojsvicetmp_XXXXXX";
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

namespace {

void unlinkFile(const PathStr& path) {
#ifdef _WIN32
    if(!DeleteFileW(path.c_str())) {
        WARNING_LOG("Deleting temporary file ", path, " failed");
    }
#else
    if(unlink(path.c_str())) {
        WARNING_LOG("Unlinking temporary file ", path, " failed");
    }
#endif
}

}

class FileUpload::Impl {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(
        CKey,
        shared_ptr<UploadStorage> storage,
        string name,
        PathStr path,
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
    PathStr path_;
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

const PathStr& FileUpload::path() {
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

    PathStr path = tempDir_->path() + PathSep + toPathStr(idx);

    ofstream fp;
    fp.open(path, ofstream::binary);
    REQUIRE(fp.good());

    SHA256_CTX hasher;
    SHA256_Init(&hasher);

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

            SHA256_Update(&hasher, buf, readCount);

            fp.write(buf, readCount);
            REQUIRE(fp.good());
        }
    } catch(...) {
        WARNING_LOG("Reading file upload stream failed with exception");
        ok = false;
    }

    unsigned char hashBin[SHA256_DIGEST_LENGTH];
    SHA256_Final(hashBin, &hasher);

    fp.close();
    REQUIRE(fp.good());

    if(!ok) {
        unlinkFile(path);
        shared_ptr<FileUpload> empty;
        return empty;
    }

    string hash;
    for(size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        int byte = (int)hashBin[i];
        for(int nibble : {byte >> 4, byte & 15}) {
            hash.push_back(nibble < 10 ? (char)('0' + nibble) : (char)('a' + (nibble - 10)));
        }
    }

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
