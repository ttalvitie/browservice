#include "key.hpp"

namespace retrojsvice {

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

const string validNonCharKeyList = initValidNonCharKeyList();

}
