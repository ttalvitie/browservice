#include "key.hpp"

namespace browservice {

namespace {

vector<int> initSortedValidNonCharKeys() {
    vector<int> ret = {
        keys::Backspace,
        keys::Tab,
        keys::Enter,
        keys::Shift,
        keys::Control,
        keys::Alt,
        keys::CapsLock,
        keys::Esc,
        keys::Space,
        keys::PageUp,
        keys::PageDown,
        keys::End,
        keys::Home,
        keys::Left,
        keys::Up,
        keys::Right,
        keys::Down,
        keys::Insert,
        keys::Delete,
        keys::Win,
        keys::Menu,
        keys::F1,
        keys::F2,
        keys::F3,
        keys::F4,
        keys::F5,
        keys::F6,
        keys::F7,
        keys::F8,
        keys::F9,
        keys::F10,
        keys::F11,
        keys::F12,
        keys::NumLock
    };
    sort(ret.begin(), ret.end());
    REQUIRE(ret.back() < 0);
    return ret;
}

vector<int> sortedValidNonCharKeys = initSortedValidNonCharKeys();

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
    REQUIRE(isValidKey(key));

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
        REQUIRE(key <= 0x10FFFF);
        ret.length = 4;
        ret.data[3] = (uint8_t)(getBits(6) | 0x80);
        ret.data[2] = (uint8_t)(getBits(6) | 0x80);
        ret.data[1] = (uint8_t)(getBits(6) | 0x80);
        ret.data[0] = (uint8_t)(key | 0xF0);
    }
    return ret;
}

}
