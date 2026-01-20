#define DUAL_KEY_CODE(ch1, ch2, win, nat) \
    {(int)ch1, {win, nat}}, {(int)ch2, {win, nat}},

// (windows key code, native key code) pairs for some keys
const map<int, pair<int, int>> keyCodes = {
    DUAL_KEY_CODE('a', 'A', 65, 38)
    DUAL_KEY_CODE('b', 'B', 66, 56)
    DUAL_KEY_CODE('c', 'C', 67, 54)
    DUAL_KEY_CODE('d', 'D', 68, 40)
    DUAL_KEY_CODE('e', 'E', 69, 26)
    DUAL_KEY_CODE('f', 'F', 70, 41)
    DUAL_KEY_CODE('g', 'G', 71, 42)
    DUAL_KEY_CODE('h', 'H', 72, 43)
    DUAL_KEY_CODE('i', 'I', 73, 31)
    DUAL_KEY_CODE('j', 'J', 74, 44)
    DUAL_KEY_CODE('k', 'K', 75, 45)
    DUAL_KEY_CODE('l', 'L', 76, 46)
    DUAL_KEY_CODE('m', 'M', 77, 58)
    DUAL_KEY_CODE('n', 'N', 78, 57)
    DUAL_KEY_CODE('o', 'O', 79, 32)
    DUAL_KEY_CODE('p', 'P', 80, 33)
    DUAL_KEY_CODE('q', 'Q', 81, 24)
    DUAL_KEY_CODE('r', 'R', 82, 27)
    DUAL_KEY_CODE('s', 'S', 83, 39)
    DUAL_KEY_CODE('t', 'T', 84, 28)
    DUAL_KEY_CODE('u', 'U', 85, 30)
    DUAL_KEY_CODE('v', 'V', 86, 55)
    DUAL_KEY_CODE('w', 'W', 87, 25)
    DUAL_KEY_CODE('x', 'X', 88, 53)
    DUAL_KEY_CODE('y', 'Y', 89, 29)
    DUAL_KEY_CODE('z', 'Z', 90, 52)

    DUAL_KEY_CODE('`', '~', 192, 49)
    DUAL_KEY_CODE('1', '!', 49, 10)
    DUAL_KEY_CODE('2', '@', 50, 11)
    DUAL_KEY_CODE('3', '#', 51, 12)
    DUAL_KEY_CODE('4', '$', 52, 13)
    DUAL_KEY_CODE('5', '%', 53, 14)
    DUAL_KEY_CODE('6', '^', 54, 15)
    DUAL_KEY_CODE('7', '&', 55, 16)
    DUAL_KEY_CODE('8', '*', 56, 17)
    DUAL_KEY_CODE('9', '(', 57, 18)
    DUAL_KEY_CODE('0', ')', 48, 19)
    DUAL_KEY_CODE('-', '_', 189, 20)
    DUAL_KEY_CODE('=', '+', 187, 21)

    DUAL_KEY_CODE('[', '{', 219, 34)
    DUAL_KEY_CODE(']', '}', 221, 35)
    DUAL_KEY_CODE('\\', '|', 220, 51)
    DUAL_KEY_CODE(';', ':', 186, 47)
    DUAL_KEY_CODE('\'', '"', 222, 48)
    DUAL_KEY_CODE(',', '<', 188, 59)
    DUAL_KEY_CODE('.', '>', 190, 60)
    DUAL_KEY_CODE('/', '?', 191, 61)

#ifdef __APPLE__
    {keys::Backspace, {8, 51}},
#else
    {keys::Backspace, {8, 22}},
#endif
    {keys::Tab, {9, 23}},
    {keys::Enter, {13, 36}},
    {keys::Shift, {16, 50}},
    {keys::Control, {17, 37}},
    {keys::Alt, {18, 64}},
    {keys::CapsLock, {20, 66}},
    {keys::Esc, {27, 9}},
    {keys::Space, {32, 65}},
    {keys::PageUp, {33, 112}},
    {keys::PageDown, {34, 117}},
    {keys::Home, {36, 110}},
    {keys::End, {35, 115}},
    {keys::Left, {37, 113}},
    {keys::Up, {38, 11}},
    {keys::Right, {39, 114}},
    {keys::Down, {40, 116}},
    {keys::Insert, {45, 118}},
    {keys::Delete, {46, 119}},
    {keys::Win, {91, 133}},
    {keys::Menu, {93, 135}},
    {keys::F1, {112, 67}},
    {keys::F2, {113, 68}},
    {keys::F3, {114, 69}},
    {keys::F4, {115, 70}},
    {keys::F5, {116, 71}},
    {keys::F6, {117, 72}},
    {keys::F7, {118, 73}},
    {keys::F8, {119, 74}},
    {keys::F9, {120, 75}},
    {keys::F10, {121, 76}},
    {keys::F11, {122, 95}},
    {keys::F12, {123, 96}},
    {keys::NumLock, {144, 77}}
};

#undef DUAL_KEY_CODE
