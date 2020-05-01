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

    friend Key createKeyUnchecked_(int id);
};

// Namespace containing all the supported keys
namespace keys {

extern const Key Space;
extern const Key A;
extern const Key a;

}

// String containing a comma-separated list of the negations of supported
// non-character key IDs
extern const string supportedNonCharKeyList;
