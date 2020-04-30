#include "key.hpp"

optional<Key> Key::fromID(int id) {
    // Only support spacebar and A for now
    if(id == 97) {
        id = 65;
    }
    if(id == -32 || id == 65) {
        return Key(id);
    }
    return {};
}

Key::Key(int id) {
    id_ = id;
}

const char* Key::name() const {
    if(id_ == -32) {
        return "Space";
    }
    if(id_ == 65) {
        return "A";
    }
    CHECK(false);
    return "";
}

Key createKeyUnchecked_(int id) {
    return Key(id);
}

namespace keys {

using namespace key_;

const Key Space = createKeyUnchecked_(-32);
const Key A = createKeyUnchecked_(65);

}

const string supportedNonCharKeyList = "32";
