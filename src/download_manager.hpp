#pragma once

#include "common.hpp"

class CefDownloadHandler;
class CefBeforeDownloadCallback;
class CefDownloadItemCallback;

namespace browservice {

class TempDir;

class CompletedDownload : public enable_shared_from_this<CompletedDownload> {
SHARED_ONLY_CLASS(CompletedDownload);
public:
    CompletedDownload(CKey,
        shared_ptr<TempDir> tempDir,
        PathStr path,
        string name
    );
    ~CompletedDownload();

    PathStr path();
    string name();

private:
    shared_ptr<TempDir> tempDir_;
    PathStr path_;
    string name_;
};

class DownloadManagerEventHandler {
public:
    virtual void onPendingDownloadCountChanged(int count) = 0;
    virtual void onDownloadProgressChanged(vector<int> progress) {}
    virtual void onDownloadCompleted(shared_ptr<CompletedDownload> file) = 0;
};

class DownloadManager : public enable_shared_from_this<DownloadManager> {
SHARED_ONLY_CLASS(DownloadManager);
public:
    DownloadManager(CKey, weak_ptr<DownloadManagerEventHandler> eventHandler);
    ~DownloadManager();

    void acceptPendingDownload();

    // Creates a new CefDownloadHandler than passes the received downloads to
    // this object (the returned object retains a pointer to this object)
    CefRefPtr<CefDownloadHandler> createCefDownloadHandler();

private:
    class DownloadHandler;

    struct DownloadInfo {
        int fileIdx;
        string name;
        CefRefPtr<CefBeforeDownloadCallback> startCallback;
        CefRefPtr<CefDownloadItemCallback> cancelCallback;
        int progress;
    };

    PathStr getFilePath_(int fileIdx);
    void unlinkFile_(int fileIdx);

    void pendingDownloadCountChanged_();
    void downloadProgressChanged_();

    weak_ptr<DownloadManagerEventHandler> eventHandler_;

    shared_ptr<TempDir> tempDir_;

    int nextFileIdx_;
    map<uint32_t, DownloadInfo> infos_;
    queue<uint32_t> pending_;
};

}
