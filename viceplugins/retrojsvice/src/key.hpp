#pragma once

#include "common.hpp"

namespace retrojsvice {

// Keys are represented by integers; positive integers are Unicode code points
// and negative integers are Windows key codes for non-character keys.
bool isValidKey(int key);

// String containing a comma-separated list of the negations of valid negative
// (that is, non-character) key IDs
extern const string validNonCharKeyList;

namespace keys {

constexpr int Backspace = -8;
constexpr int Tab = -9;
constexpr int Enter = -13;
constexpr int Shift = -16;
constexpr int Control = -17;
constexpr int Alt = -18;
constexpr int CapsLock = -20;
constexpr int Esc = -27;
constexpr int Space = -32;
constexpr int PageUp = -33;
constexpr int PageDown = -34;
constexpr int End = -35;
constexpr int Home = -36;
constexpr int Left = -37;
constexpr int Up = -38;
constexpr int Right = -39;
constexpr int Down = -40;
constexpr int Insert = -45;
constexpr int Delete = -46;
constexpr int Win = -91;
constexpr int Menu = -93;
constexpr int F1 = -112;
constexpr int F2 = -113;
constexpr int F3 = -114;
constexpr int F4 = -115;
constexpr int F5 = -116;
constexpr int F6 = -117;
constexpr int F7 = -118;
constexpr int F8 = -119;
constexpr int F9 = -120;
constexpr int F10 = -121;
constexpr int F11 = -122;
constexpr int F12 = -123;
constexpr int NumLock = -144;

}

}
