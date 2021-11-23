#include "download_manager.hpp"

#include "temp_dir.hpp"

#include "include/cef_download_handler.h"

namespace browservice {

CompletedDownload::CompletedDownload(CKey,
    shared_ptr<TempDir> tempDir,
    PathStr path,
    string name
) {
    tempDir_ = tempDir;
    path_ = move(path);
    name_ = move(name);
}

CompletedDownload::~CompletedDownload() {
#ifdef _WIN32
    if(!DeleteFileW(path_.c_str())) {
        WARNING_LOG("Deleting file ", path_, " failed");
    }
#else
    if(unlink(path_.c_str())) {
        WARNING_LOG("Unlinking file ", path_, " failed");
    }
#endif
}

PathStr CompletedDownload::path() {
    REQUIRE_UI_THREAD();
    return path_;
}

string CompletedDownload::name() {
    REQUIRE_UI_THREAD();
    return name_;
}

class DownloadManager::DownloadHandler : public CefDownloadHandler {
public:
    DownloadHandler(shared_ptr<DownloadManager> downloadManager) {
        downloadManager_ = downloadManager;
    }

    // CefDownloadHandler:
    virtual void OnBeforeDownload(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDownloadItem> downloadItem,
        const CefString& suggestedName,
        CefRefPtr<CefBeforeDownloadCallback> callback
    ) override {
        REQUIRE_UI_THREAD();
        REQUIRE(downloadItem->IsValid());

        uint32_t id = downloadItem->GetId();
        REQUIRE(!downloadManager_->infos_.count(id));
        DownloadInfo& info = downloadManager_->infos_[id];

        info.fileIdx = downloadManager_->nextFileIdx_++;
        info.name = suggestedName;
        info.startCallback = callback;
        info.cancelCallback = nullptr;
        info.progress = 0;

        downloadManager_->pending_.push(id);
        downloadManager_->pendingDownloadCountChanged_();
    }

    virtual void OnDownloadUpdated(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDownloadItem> downloadItem,
        CefRefPtr<CefDownloadItemCallback> callback
    ) override {
        REQUIRE_UI_THREAD();
        REQUIRE(downloadItem->IsValid());

        uint32_t id = downloadItem->GetId();
        if(!downloadManager_->infos_.count(id)) {
            return;
        }
        DownloadInfo& info = downloadManager_->infos_[id];
        if(info.startCallback) {
            return;
        }
        info.cancelCallback = callback;

        if(downloadItem->IsComplete()) {
            shared_ptr<CompletedDownload> file = CompletedDownload::create(
                downloadManager_->tempDir_,
                downloadManager_->getFilePath_(info.fileIdx),
                move(info.name)
            );
            downloadManager_->infos_.erase(id);
            postTask(
                downloadManager_->eventHandler_,
                &DownloadManagerEventHandler::onDownloadCompleted,
                file
            );
        } else if(!downloadItem->IsInProgress()) {
            info.cancelCallback->Cancel();
            downloadManager_->unlinkFile_(info.fileIdx);
            downloadManager_->infos_.erase(id);
        } else {
            info.progress = downloadItem->GetPercentComplete();
            if(info.progress == -1) {
                info.progress = 50;
            }
            info.progress = max(0, min(100, info.progress));
        }
        downloadManager_->downloadProgressChanged_();
    }

private:
    shared_ptr<DownloadManager> downloadManager_;

    IMPLEMENT_REFCOUNTING(DownloadHandler);
};

DownloadManager::DownloadManager(CKey,
    weak_ptr<DownloadManagerEventHandler> eventHandler
) {
    REQUIRE_UI_THREAD();

    eventHandler_ = eventHandler;
    nextFileIdx_ = 1;
}

DownloadManager::~DownloadManager() {
    for(const auto& p : infos_) {
        const DownloadInfo& info = p.second;
        if(!info.startCallback) {
            if(info.cancelCallback) {
                info.cancelCallback->Cancel();
            }
            unlinkFile_(info.fileIdx);
        }
    }
}

void DownloadManager::acceptPendingDownload() {
    REQUIRE_UI_THREAD();

    if(!pending_.empty()) {
        uint32_t id = pending_.front();
        pending_.pop();
        pendingDownloadCountChanged_();

        REQUIRE(infos_.count(id));
        DownloadInfo& info = infos_[id];
        
        PathStr path = getFilePath_(info.fileIdx);

        REQUIRE(info.startCallback);
        info.startCallback->Continue(path, false);
        info.startCallback = nullptr;

        downloadProgressChanged_();
    }
}

CefRefPtr<CefDownloadHandler> DownloadManager::createCefDownloadHandler() {
    REQUIRE_UI_THREAD();
    return new DownloadHandler(shared_from_this());
}

PathStr DownloadManager::getFilePath_(int fileIdx) {
    if(!tempDir_) {
        tempDir_ = TempDir::create();
    }

    return tempDir_->path() + PathSep + PATHSTR("file_") + toPathStr(fileIdx) + PATHSTR(".bin");
}

void DownloadManager::unlinkFile_(int fileIdx) {
    PathStr path = getFilePath_(fileIdx);
#ifdef _WIN32
    if(!DeleteFileW(path.c_str())) {
        WARNING_LOG("Deleting file ", path, " failed");
    }
#else
    if(unlink(path.c_str())) {
        WARNING_LOG("Unlinking file ", path, " failed");
    }
#endif
}

void DownloadManager::pendingDownloadCountChanged_() {
    postTask(
        eventHandler_,
        &DownloadManagerEventHandler::onPendingDownloadCountChanged,
        (int)pending_.size()
    );
}

void DownloadManager::downloadProgressChanged_() {
    vector<pair<int, int>> pairs;
    for(const auto& elem : infos_) {
        if(!elem.second.startCallback) {
            pairs.emplace_back(elem.second.fileIdx, elem.second.progress);
        }
    }
    sort(pairs.begin(), pairs.end());
    vector<int> progress;
    for(pair<int, int> p : pairs) {
        progress.push_back(p.second);
    }
    postTask(
        eventHandler_,
        &DownloadManagerEventHandler::onDownloadProgressChanged,
        progress
    );
}

}
