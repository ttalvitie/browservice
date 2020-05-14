#pragma once

#include "common.hpp"

// Keys are represented by integers; positive integers are Unicode code points
// and negative integers are Windows key codes for non-character keys.
bool isValidKey(int key);

struct UTF8Char {
    uint8_t data[4];
    int length;
};

// Returns the UTF-8 representation of the character represented by given valid
// key. If key < 0, the return value has length equal to 0.
UTF8Char keyToUTF8(int key);

// String containing a comma-separated list of the negations of valid negative
// (that is, non-character) key IDs
extern const string validNonCharKeyList;

namespace keys {

constexpr int Backspace = -8;
constexpr int Tab = -9;
constexpr int Enter = -13;
constexpr int Shift = -16;
constexpr int Space = -32;
constexpr int PageUp = -33;
constexpr int PageDown = -34;
constexpr int Left = -37;
constexpr int Up = -38;
constexpr int Right = -39;
constexpr int Down = -40;
constexpr int Delete = -46;

}
