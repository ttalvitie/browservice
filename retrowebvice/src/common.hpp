#pragma once

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace retrowebvice {

using std::atomic;
using std::cerr;
using std::exception;
using std::function;
using std::get;
using std::get_if;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::optional;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::thread;
using std::tie;
using std::tuple;
using std::unique_ptr;
using std::variant;
using std::vector;
using std::weak_ptr;

using std::chrono::milliseconds;

using std::this_thread::sleep_for;

template <typename T>
optional<T> parseString(const string& str) {
    T ret;
    stringstream ss(str);
    ss >> ret;
    if(ss.fail() || !ss.eof()) {
        optional<T> empty;
        return empty;
    } else {
        return ret;
    }
}

template <typename T>
string toString(const T& obj) {
    stringstream ss;
    ss << obj;
    return ss.str();
}

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

}
