#include "download_manager.hpp"

#include "http.hpp"
#include "temp_dir.hpp"

#include "include/cef_download_handler.h"

namespace {

pair<string, string> extractExtension(const string& filename) {
    int lastDot = (int)filename.size() - 1;
    while(lastDot >= 0 && filename[lastDot] != '.') {
        --lastDot;
    }
    if(lastDot >= 0) {
        int extLength = (int)filename.size() - 1 - lastDot;
        if(extLength >= 1 && extLength <= 5) {
            bool ok = true;
            for(int i = lastDot + 1; i < (int)filename.size(); ++i) {
                char c = filename[i];
                if(!(
                    (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9')
                )) {
                    ok = false;
                    break;
                }
            }
            if(ok) {
                return make_pair(
                    filename.substr(0, lastDot),
                    filename.substr(lastDot + 1)
                );
            }
        }
    }
    return make_pair(filename, "bin");
}

string sanitizeBase(const string& base) {
    string ret;
    for(char c : base) {
        if(
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')
        ) {
            ret.push_back(c);
        } else {
            if(!ret.empty() && ret.back() != '_') {
                ret.push_back('_');
            }
        }
    }
    if(ret.empty() || !(
        (ret[0] >= 'a' && ret[0] <= 'z') ||
        (ret[0] >= 'A' && ret[0] <= 'Z')
    )) {
        ret = "file_" + ret;
    }
    ret = ret.substr(0, 32);
    if(ret.back() == '_') {
        ret.pop_back();
    }
    return ret;
}

string sanitizeFilename(const string& filename) {
    string base, ext;
    tie(base, ext) = extractExtension(filename);

    string sanitizedBase = sanitizeBase(base);

    return sanitizedBase + "." + ext;
}

}

CompletedDownload::CompletedDownload(CKey,
    shared_ptr<TempDir> tempDir,
    string path,
    string name,
    uint64_t length
) {
    tempDir_ = tempDir;
    path_ = move(path);
    name_ = move(name);
    length_ = length;
}

CompletedDownload::~CompletedDownload() {
    if(unlink(path_.c_str())) {
        WARNING_LOG("Unlinking file ", path_, " failed");
    }
}

string CompletedDownload::name() {
    REQUIRE_UI_THREAD();
    return name_;
}

void CompletedDownload::serve(shared_ptr<HTTPRequest> request) {
    REQUIRE_UI_THREAD();

    shared_ptr<CompletedDownload> self = shared_from_this();
    function<void(ostream&)> body = [self](ostream& out) {
        ifstream fp;
        fp.open(self->path_, ifstream::binary);

        if(!fp.good()) {
            ERROR_LOG("Opening downloaded file ", self->path_, " failed");
            return;
        }

        const uint64_t BufSize = 1 << 16;
        char buf[BufSize];

        uint64_t left = self->length_;
        while(left) {
            uint64_t readSize = min(left, BufSize);
            fp.read(buf, readSize);

            if(!fp.good()) {
                ERROR_LOG("Reading downloaded file ", self->path_, " failed");
                return;
            }

            out.write(buf, readSize);
            left -= readSize;
        }
        fp.close();
    };

    request->sendResponse(
        200,
        "application/download",
        length_,
        body,
        false,
        {{"Content-Disposition", "attachment; filename=\"" + name_ + "\""}}
    );
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
        info.name = sanitizeFilename(suggestedName);
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
            int64_t length = downloadItem->GetReceivedBytes();
            REQUIRE(length >= 0);

            shared_ptr<CompletedDownload> file = CompletedDownload::create(
                downloadManager_->tempDir_,
                downloadManager_->getFilePath_(info.fileIdx),
                move(info.name),
                (uint64_t)length
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
    for(const pair<uint32_t, DownloadInfo>& p : infos_) {
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
        
        string path = getFilePath_(info.fileIdx);

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

string DownloadManager::getFilePath_(int fileIdx) {
    if(!tempDir_) {
        tempDir_ = TempDir::create();
    }

    return tempDir_->path() + "/file_" + toString(fileIdx) + ".bin";
}

void DownloadManager::unlinkFile_(int fileIdx) {
    string path = getFilePath_(fileIdx);
    if(unlink(path.c_str())) {
        WARNING_LOG("Unlinking file ", path, " failed");
    }
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
    for(const pair<uint32_t, DownloadInfo>& elem : infos_) {
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
