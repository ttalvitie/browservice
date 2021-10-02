/*
#pragma once

#include "common.hpp"

class CefRequest;

namespace browservice {

struct Bookmark {
    string url;
    string title;
    uint64_t time;
};

class Bookmarks {
SHARED_ONLY_CLASS(Bookmarks);
public:
    Bookmarks(CKey);

    // Returns empty pointer and writes to log if loading failed
    static shared_ptr<Bookmarks> load();

    // Returns false and writes to log if saving failed
    bool save();

    const map<uint64_t, Bookmark>& getData() const;

    uint64_t putBookmark(Bookmark bookmark);
    void removeBookmark(uint64_t id);

private:
    map<uint64_t, Bookmark> data_;
};

// Returns the bookmark ID if given url is bookmarked in the last state saved
// using Bookmarks::save, or if no bookmarks have yet been saved, the state
// loaded on the first call to this function. Safe to call from any thread.
optional<uint64_t> getCachedBookmarkIDByURL(string url);

string handleBookmarksRequest(CefRefPtr<CefRequest> request);

}
*/
