#include "download.hpp"

#include "http.hpp"

namespace retrojsvice {

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

optional<uint64_t> getFileSize(const string& path) {
    ifstream fp;
    fp.open(path, ifstream::binary);
    if(!fp.good()) return {};
    fp.seekg(0, ifstream::end);
    if(!fp.good()) return {};
    uint64_t size = fp.tellg();
    if(!fp.good()) return {};
    fp.close();
    return size;
}

}

FileDownload::FileDownload(CKey,
    string name, string path, function<void()> cleanup
) {
    REQUIRE_API_THREAD();

    name_ = sanitizeFilename(name);
    path_ = move(path);
    cleanup_ = move(cleanup);
}

FileDownload::~FileDownload() {
    cleanup_();
}

string FileDownload::name() {
    return name_;
}

void FileDownload::serve(shared_ptr<HTTPRequest> request) {
    REQUIRE_API_THREAD();

    optional<uint64_t> lengthOpt = getFileSize(path_);
    if(!lengthOpt) {
        ERROR_LOG("Determining the size of downloaded file ", path_, " failed");
        request->sendTextResponse(500, "ERROR: Internal server error\n");
        return;
    }
    uint64_t length = *lengthOpt;

    shared_ptr<FileDownload> self = shared_from_this();
    function<void(ostream&)> body = [self, length](ostream& out) {
        ifstream fp;
        fp.open(self->path_, ifstream::binary);

        if(!fp.good()) {
            ERROR_LOG("Opening downloaded file ", self->path_, " failed");
            return;
        }

        const uint64_t BufSize = 1 << 16;
        char buf[BufSize];

        uint64_t left = length;
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
        length,
        body,
        false,
        {{"Content-Disposition", "attachment; filename=\"" + name_ + "\""}}
    );
}

}
