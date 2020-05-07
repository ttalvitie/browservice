#include "key.hpp"

optional<Key> Key::fromID(int id) {
    Key key(id);
    if(SupportedKeys_.count(key)) {
        return key;
    } else {
        return {};
    }
}

Key::Key(int id) {
    id_ = id;
}

const char* Key::name() const {
    auto it = SupportedKeys_.find(*this);
    CHECK(it != SupportedKeys_.end());
    return it->second.name;
}

const char* Key::character() const {
    auto it = SupportedKeys_.find(*this);
    CHECK(it != SupportedKeys_.end());
    return it->second.character;
}


map<Key, Key::Info> Key::initSupportedKeys_() {
    map<Key, Info> ret;

#define KEY_DEF(name, id, character) \
    { \
        Key key(id); \
        Info info = {#name, character}; \
        CHECK((character[0] == '\0') == (id < 0)); \
        CHECK(ret.emplace(key, info).second); \
    }

#include "key_defs.hpp"

#undef KEY_DEF

    return ret;
}

const map<Key, Key::Info> Key::SupportedKeys_ = Key::initSupportedKeys_();

Key createKeyUnchecked_(int id) {
    return Key(id);
}

namespace keys {

#define KEY_DEF(name, id, character) \
    const Key name = createKeyUnchecked_(id);

#include "key_defs.hpp"

#undef KEY_DEF

}

static string initSupportedNonCharKeyList() {
    stringstream ss;
    bool first = true;

#define KEY_DEF(name, id, character) \
    if(id < 0) { \
        if(!first) { \
            ss << ","; \
        } \
        first = false; \
        ss << -id; \
    }

#include "key_defs.hpp"

#undef KEY_DEF

    return ss.str();
}

const string supportedNonCharKeyList = initSupportedNonCharKeyList();
