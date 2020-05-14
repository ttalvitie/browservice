#include "key.hpp"

namespace {

vector<int> initSortedValidNonCharKeys() {
    vector<int> ret = {
        keys::Backspace,
        keys::Tab,
        keys::Enter,
        keys::Shift,
        keys::Control,
        keys::Alt,
        keys::Space,
        keys::PageUp,
        keys::PageDown,
        keys::Left,
        keys::Up,
        keys::Right,
        keys::Down,
        keys::Delete
    };
    sort(ret.begin(), ret.end());
    return ret;
}

vector<int> sortedValidNonCharKeys = initSortedValidNonCharKeys();

string initValidNonCharKeyList() {
    stringstream ss;
    bool first = true;
    for(int i = (int)sortedValidNonCharKeys.size() - 1; i >= 0; --i) {
        if(!first) {
            ss << ",";
        }
        first = false;
        ss << -sortedValidNonCharKeys[i];
    }
    return ss.str();
}

}

bool isValidKey(int key) {
    return
        (key >= 1 && key <= 0xD7FF) ||
        (key >= 0xE000 && key <= 0x10FFFF) ||
        binary_search(
            sortedValidNonCharKeys.begin(),
            sortedValidNonCharKeys.end(),
            key
        );
}

UTF8Char keyToUTF8(int key) {
    CHECK(isValidKey(key));

    auto getBits = [&](int count) {
        int bits = key & ((1 << count) - 1);
        key >>= count;
        return bits;
    };

    UTF8Char ret;
    if(key < 0) {
        ret.length = 0;
    } else if(key <= 0x7F) {
        ret.length = 1;
        ret.data[0] = (uint8_t)key;
    } else if(key <= 0x7FF) {
        ret.length = 2;
        ret.data[1] = (uint8_t)(getBits(6) | 0x80);
        ret.data[0] = (uint8_t)(key | 0xC0);
    } else if(key <= 0xFFFF) {
        ret.length = 3;
        ret.data[2] = (uint8_t)(getBits(6) | 0x80);
        ret.data[1] = (uint8_t)(getBits(6) | 0x80);
        ret.data[0] = (uint8_t)(key | 0xE0);
    } else {
        CHECK(key <= 0x10FFFF);
        ret.length = 4;
        ret.data[3] = (uint8_t)(getBits(6) | 0x80);
        ret.data[2] = (uint8_t)(getBits(6) | 0x80);
        ret.data[1] = (uint8_t)(getBits(6) | 0x80);
        ret.data[0] = (uint8_t)(key | 0xF0);
    }
    return ret;
}

const string validNonCharKeyList = initValidNonCharKeyList();
