#pragma once

#include "common.hpp"

namespace retrojsvice {

class TempDir {
SHARED_ONLY_CLASS(TempDir);
public:
    TempDir(CKey);
    ~TempDir();

    const string& path();

private:
    string path_;
};

class FileUpload {
SHARED_ONLY_CLASS(FileUpload);
private:
    class Impl;

public:
    // Private constructor.
    FileUpload(CKey, shared_ptr<Impl> impl);

    ~FileUpload();

    const string& path();
    const string& name();

private:
    shared_ptr<Impl> impl_;

    friend class UploadStorage;
};

// Shared storage for file uploads that deduplicates files that have the same
// name and content. UploadStorages and FileUploads are thread-safe.
class UploadStorage : public enable_shared_from_this<UploadStorage> {
SHARED_ONLY_CLASS(UploadStorage);
public:
    UploadStorage(CKey);

    ~UploadStorage();

    // Returns the file upload object if the reading the file from dataStream
    // succeeds (badbit is not raised and no exceptions are thrown). Otherwise,
    // returns an empty pointer.
    shared_ptr<FileUpload> upload(string name, istream& dataStream);

private:
    shared_ptr<TempDir> tempDir_;
    atomic<uint64_t> nextIdx_;
    mutex mutex_;
    map<pair<string, string>, weak_ptr<FileUpload::Impl>> files_;

    friend class FileUpload;
};

string extractUploadFilename(string src);

}
