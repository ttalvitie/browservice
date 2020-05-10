#pragma once

#include "common.hpp"

class DownloadManagerEventHandler {
public:
    virtual void onPendingDownloadCountChanged(int count) = 0;
    virtual void onDownloadProgressChanged(vector<int> progress) {}
};

class TempDir;

class CefDownloadHandler;
class CefBeforeDownloadCallback;
class CefDownloadItemCallback;

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

    string getFilePath_(int fileIdx);
    void unlinkFile_(int fileIdx);

    void pendingDownloadCountChanged_();
    void downloadProgressChanged_();

    weak_ptr<DownloadManagerEventHandler> eventHandler_;

    shared_ptr<TempDir> tempDir_;

    int nextFileIdx_;
    map<uint32_t, DownloadInfo> infos_;
    queue<uint32_t> pending_;
};
