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

class FileUpload;

// Shared storage for file uploads that deduplicates files that have the same
// name and content. UploadStorages and FileUploads are thread-safe.
class UploadStorage {
SHARED_ONLY_CLASS(UploadStorage);
public:
    UploadStorage(CKey);

    // Returns the file upload object if the reading the file from dataStream
    // succeeds (badbit is not raised and no exceptions are thrown). Otherwise,
    // returns an empty pointer.
    shared_ptr<FileUpload> upload(string name, istream& dataStream);

private:
    shared_ptr<TempDir> tempDir_;

    friend class FileUpload;
};

class FileUpload {
SHARED_ONLY_CLASS(FileUpload);
public:
    // Private constructor.
    FileUpload(CKey, CKey);

    const string& path();

private:
    shared_ptr<UploadStorage> storage_;
    string path_;

    friend class UploadStorage;
};

}
