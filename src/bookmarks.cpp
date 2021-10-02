/*
#include "bookmarks.hpp"

#include "globals.hpp"

#include "include/cef_request.h"

#include <sys/stat.h>
#include <sys/types.h>

namespace browservice {

namespace {

mutex bookmarkCacheMutex;
bool bookmarkCacheInitialized = false;
std::map<string, uint64_t> bookmarkCache;

// Caller needs to lock bookmarkCacheMutex
void populateBookmarkCache(const Bookmarks& bookmarks) {
    bookmarkCache.clear();
    for(const auto& item : bookmarks.getData()) {
        bookmarkCache[item.second.url] = item.first;
    }
    bookmarkCacheInitialized = true;
}

bool tryCreateDotDir() {
    if(mkdir(globals->dotDirPath.c_str(), 0700) == 0) {
        return true;
    } else {
        if(errno == EEXIST) {
            struct stat st;
            if(stat(globals->dotDirPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                return true;
            }
        }
        return false;
    }
}

void writeLE(ofstream& fp, uint64_t val) {
    uint8_t bytes[8];
    for(int i = 0; i < 8; ++i) {
        bytes[i] = (uint8_t)val;
        val >>= 8;
    }
    fp.write((char*)bytes, 8);
}

void writeStr(ofstream& fp, const string& val) {
    writeLE(fp, val.size());
    fp.write(val.data(), val.size());

    size_t padCount = (-val.size()) & (size_t)7;
    if(padCount) {
        char zeros[8] = {};
        fp.write(zeros, padCount);
    }
}

bool readLE(ifstream& fp, uint64_t& val) {
    uint8_t bytes[8];
    fp.read((char*)bytes, 8);
    if(!fp.good()) {
        return false;
    }

    val = 0;
    for(int i = 7; i >= 0; --i) {
        val <<= 8;
        val |= (uint64_t)bytes[i];
    }
    return true;
}

bool readStr(ifstream& fp, string& val) {
    uint64_t size;
    if(!readLE(fp, size)) {
        return false;
    }

    size_t padCount = (-size) & (size_t)7;
    size_t paddedSize = size + padCount;

    vector<char> buf(paddedSize);
    fp.read(buf.data(), paddedSize);
    if(!fp.good()) {
        return false;
    }

    val.assign(buf.begin(), buf.begin() + size);
    return true;
}

}

Bookmarks::Bookmarks(CKey) {}

shared_ptr<Bookmarks> Bookmarks::load() {
    if(!tryCreateDotDir()) {
        ERROR_LOG(
            "Loading bookmarks failed: "
            "Directory '", globals->dotDirPath, "' does not exist and creating it failed"
        );
        return {};
    }

    string bookmarkPath = globals->dotDirPath + "/bookmarks";

    shared_ptr<Bookmarks> ret = Bookmarks::create();

    struct stat st;
    if(stat(bookmarkPath.c_str(), &st) == -1 && errno == ENOENT) {
        INFO_LOG("Bookmark file '", bookmarkPath, "' does not exist, using empty set of bookmarks");
        return ret;
    }

    auto readError = [&]() -> shared_ptr<Bookmarks> {
        ERROR_LOG(
            "Loading bookmarks failed: Reading file '", bookmarkPath, "' failed"
        );
        return {};
    };
    auto formatError = [&]() -> shared_ptr<Bookmarks> {
        ERROR_LOG(
            "Loading bookmarks failed: File '", bookmarkPath, "' has invalid format"
        );
        return {};
    };

    ifstream fp;
    fp.open(bookmarkPath);
    if(!fp.good()) return readError();

    uint64_t signature;
    if(!readLE(fp, signature)) return readError();
    if(signature != 0xBA0F5EAF1CEB00C3) return formatError();

    uint64_t version;
    if(!readLE(fp, version)) return readError();
    if(version != (uint64_t)0) return formatError();

    while(true) {
        uint64_t hasNext;
        if(!readLE(fp, hasNext)) return readError();
        if(hasNext != (uint64_t)0 && hasNext != (uint64_t)1) return formatError();

        if(!hasNext) {
            break;
        }

        uint64_t id;
        Bookmark bookmark;
        if(!readLE(fp, id) || !readStr(fp, bookmark.url) || !readStr(fp, bookmark.title) || !readLE(fp, bookmark.time)) {
            return readError();
        }

        if(!ret->data_.emplace(id, move(bookmark)).second) {
            return formatError();
        }
    }

    fp.close();

    INFO_LOG("Bookmarks successfully read from '", bookmarkPath, "'");
    return ret;
}

bool Bookmarks::save() {
    if(!tryCreateDotDir()) {
        ERROR_LOG(
            "Saving bookmarks failed: "
            "Directory '", globals->dotDirPath, "' does not exist and creating it failed"
        );
        return false;
    }

    string bookmarkPath = globals->dotDirPath + "/bookmarks";

    string bookmarkTmpPath = globals->dotDirPath + "/.tmp.bookmarks.";
    string charPalette = "abcdefghijklmnopqrstuvABCDEFGHIJKLMNOPQRSTUV0123456789";
    for(int i = 0; i < 16; ++i) {
        char c = charPalette[uniform_int_distribution<size_t>(0, charPalette.size() - 1)(rng)];
        bookmarkTmpPath.push_back(c);
    }

    ofstream fp;
    fp.open(bookmarkTmpPath);

    // File signature
    writeLE(fp, 0xBA0F5EAF1CEB00C3);

    // Format version
    writeLE(fp, 0);

    for(const auto& item : data_) {
        uint64_t id = item.first;
        const Bookmark& bookmark = item.second;

        // One more item
        writeLE(fp, 1);

        writeLE(fp, id);
        writeStr(fp, bookmark.url);
        writeStr(fp, bookmark.title);
        writeLE(fp, bookmark.time);
    }

    // No more items
    writeLE(fp, 0);

    fp.close();

    if(!fp.good()) {
        ERROR_LOG(
            "Saving bookmarks failed: "
            "Could not write temporary file '", bookmarkTmpPath, "'"
        );
        return false;
    }

    if(rename(bookmarkTmpPath.c_str(), bookmarkPath.c_str()) != 0) {
        ERROR_LOG(
            "Saving bookmarks failed: "
            "Renaming temporary file '", bookmarkTmpPath, "' to '", bookmarkPath, "' failed"
        );
        unlink(bookmarkTmpPath.c_str());
        return false;
    }

    INFO_LOG("Bookmarks successfully written to '", bookmarkPath, "'");

    {
        lock_guard lock(bookmarkCacheMutex);
        populateBookmarkCache(*this);
    }

    return true;
}

const map<uint64_t, Bookmark>& Bookmarks::getData() const {
    return data_;
}

uint64_t Bookmarks::putBookmark(Bookmark bookmark) {
    uint64_t id;
    do {
        id = uniform_int_distribution<uint64_t>()(rng);
    } while(data_.count(id));

    data_.emplace(id, move(bookmark));
    return id;
}

void Bookmarks::removeBookmark(uint64_t id) {
    data_.erase(id);
}

optional<uint64_t> getCachedBookmarkIDByURL(string url) {
    lock_guard lock(bookmarkCacheMutex);
    if(!bookmarkCacheInitialized) {
        shared_ptr<Bookmarks> bookmarks = Bookmarks::load();
        if(bookmarks) {
            populateBookmarkCache(*bookmarks);
        } else {
            bookmarkCache.clear();
            bookmarkCacheInitialized = true;
        }
    }
    auto it = bookmarkCache.find(url);
    if(it == bookmarkCache.end()) {
        optional<uint64_t> empty;
        return empty;
    } else {
        return it->second;
    }
}

namespace {

string htmlEscapeString(string str) {
    string ret;
    for(int point : sanitizeUTF8StringToCodePoints(move(str))) {
        if(point <= 0 || point > 0x10FFFF) continue;
        if(point == 0xD) continue;
        if(
            (point <= 0x1F || (point >= 0x7F && point <= 0x9F)) &&
            point != 0x20 && point != 0x9 && point != 0xA && point != 0xC
        ) continue;
        if(point >= 0xFDD0 && point <= 0xFDEF) continue;
        if((point & 0xFFFF) == 0xFFFE || (point & 0xFFFF) == 0xFFFF) continue;
        ret += "&#" + toString(point) + ";";
    }
    return ret;
}

void parseSelectedBookmarks(string formData, std::set<uint64_t>& selectedBookmarks) {
    if(formData.empty()) {
        return;
    }

    size_t start = 0;
    while(true) {
        size_t end = formData.find('&', start);
        bool isLast = (end == string::npos);
        if(isLast) {
            end = formData.size();
        }

        string elem = formData.substr(start, end - start);
        const string prefix = "bookmark";
        const string suffix = "=on";
        if(
            elem.size() > prefix.size() + suffix.size() &&
            elem.substr(0, prefix.size()) == prefix &&
            elem.substr(elem.size() - suffix.size()) == suffix
        ) {
            optional<uint64_t> id = parseString<uint64_t>(
                elem.substr(prefix.size(), elem.size() - prefix.size() - suffix.size())
            );
            if(id.has_value()) {
                selectedBookmarks.insert(id.value());
            }
        }

        if(isLast) {
            return;
        }
        start = end + 1;
    }
}

}

string handleBookmarksRequest(CefRefPtr<CefRequest> request) {
    CEF_REQUIRE_IO_THREAD();

    shared_ptr<Bookmarks> bookmarks = Bookmarks::load();

    if(request->GetMethod() == "POST") {
        CefRefPtr<CefPostData> postData = request->GetPostData();
        std::set<uint64_t> selectedBookmarks;
        if(postData) {
            std::vector<CefRefPtr<CefPostDataElement>> elements;
            postData->GetElements(elements);
            for(CefRefPtr<CefPostDataElement> elem : elements) {
                if(elem->GetType() == PDE_TYPE_BYTES) {
                    size_t dataSize = elem->GetBytesCount();
                    std::vector<char> data(dataSize);
                    if(elem->GetBytes(dataSize, data.data()) == dataSize) {
                        parseSelectedBookmarks(string(data.begin(), data.end()), selectedBookmarks);
                    }
                }
            }
        }
        if(!selectedBookmarks.empty() && bookmarks) {
            for(uint64_t id : selectedBookmarks) {
                bookmarks->removeBookmark(id);
            }
            bookmarks->save();
        }
    }

    string page =
        "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"UTF-8\">"
        "<title>Bookmarks</title></head><body>\n"
        "<h1>Bookmarks</h1>\n"
        "<form action=\"browservice:bookmarks\" method=\"POST\">\n";

    if(bookmarks) {
        const map<uint64_t, Bookmark>& bookmarkData = bookmarks->getData();
        vector<const pair<const uint64_t, Bookmark>*> items;
        for(const auto& item : bookmarkData) {
            items.push_back(&item);
        }
        sort(
            items.begin(), items.end(),
            [](
                const pair<const uint64_t, Bookmark>* a,
                const pair<const uint64_t, Bookmark>* b
            ) {
                return
                    make_tuple(a->second.time, a->second.title, a->second.url) <
                    make_tuple(b->second.time, b->second.title, b->second.url);
            }
        );
        for(const auto* item : items) {
            uint64_t id = item->first;
            const Bookmark& bookmark = item->second;
            page += "<p style=\"margin-bottom:3px; margin-top:3px;\">\n";
            page +=
                "<input type=\"checkbox\" id=\"bookmark" + toString(id) + "\" "
                "name=\"bookmark" + toString(id) + "\">\n";
            page += "<label for=\"bookmark" + toString(id) + "\">\n";
            page +=
                "<a href=\"" + htmlEscapeString(bookmark.url) + "\">" +
                htmlEscapeString(bookmark.title) + "</a>\n";
            page += "</label>\n";
            page += "</p>\n";
        }
        if(items.empty()) {
            page += "<p>You have no bookmarks</p>\n";
        } else {
            page += "<p><input type=\"submit\" value=\"Remove selected\"></p>\n";
        }
    } else {
        page += "<p style=\"color:#FF0000;\">Loading bookmarks failed (see log)</p>\n";
    }

    page += "</form>\n";
    page += "</body></html>\n";
    return page;
}

}
*/
