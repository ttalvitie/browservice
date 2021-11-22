#pragma once

#include "common.hpp"

namespace retrojsvice {

class HTTPRequest;

class FileDownload : public enable_shared_from_this<FileDownload> {
SHARED_ONLY_CLASS(FileDownload);
public:
    FileDownload(CKey, string name, PathStr path, function<void()> cleanup);
    ~FileDownload();

    string name();

    // Serve the downloaded file to as response to given request. Note that
    // no-cache-headers are omitted, so the result may be cached (to circumvent
    // bugs in IE).
    void serve(shared_ptr<HTTPRequest> request);

private:
    string name_;
    PathStr path_;
    function<void()> cleanup_;
};

}
