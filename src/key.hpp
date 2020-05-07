#pragma once

#include "common.hpp"

namespace key_ {}

// Identifier of a supported key
class Key {
public:
    // Given key ID, returns either the key or empty if the key is not supported.
    // Positive key IDs refer to character keys and negative key IDs to
    // non-character keys; the absolute value should be the JS key code (code
    // point for character keys).
    static optional<Key> fromID(int id);

    // Returns string literal that is the canonical name of the key (same as in
    // the keys namespace)
    const char* name() const;

    // Returns string literal representing the character that the key produces
    // (empty if this is a non-character key)
    const char* character() const;

    #define DEFINE_KEY_OP(op) \
        bool operator op(Key other) const { \
            return id_ op other.id_; \
        }
    DEFINE_KEY_OP(==);
    DEFINE_KEY_OP(!=);
    DEFINE_KEY_OP(<);
    DEFINE_KEY_OP(>);
    DEFINE_KEY_OP(<=);
    DEFINE_KEY_OP(>=);
    #undef DEFINE_KEY_OP

private:
    Key(int id);

    int id_;

    struct Info {
        const char* name;
        const char* character;
    };

    static map<Key, Info> initSupportedKeys_();
    static const map<Key, Info> SupportedKeys_;

    friend Key createKeyUnchecked_(int id);
};

// Namespace containing all the supported keys
namespace keys {

#define KEY_DEF(name, id, character) \
    extern const Key name;

#include "key_defs.hpp"

#undef KEY_DEF

}

// String containing a comma-separated list of the negations of supported
// non-character key IDs
extern const string supportedNonCharKeyList;
