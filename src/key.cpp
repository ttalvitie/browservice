#include "key.hpp"

optional<Key> Key::fromID(int id) {
    if(id == -32 || id == -16 || id == -37 || id == -39 || id == 65 || id == 97) {
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
    if(id_ == -16) {
        return "Shift";
    }
    if(id_ == -37) {
        return "Left";
    }
    if(id_ == -39) {
        return "Right";
    }
    if(id_ == 65) {
        return "A";
    }
    if(id_ == 97) {
        return "a";
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
const Key Shift = createKeyUnchecked_(-16);
const Key Left = createKeyUnchecked_(-37);
const Key Right = createKeyUnchecked_(-39);
const Key A = createKeyUnchecked_(65);
const Key a = createKeyUnchecked_(97);

}

const string supportedNonCharKeyList = "32,16,37,39";
