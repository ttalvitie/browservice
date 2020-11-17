#pragma once

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace retrowebvice {

using std::cerr;
using std::exception;
using std::function;
using std::get;
using std::get_if;
using std::ostream;
using std::pair;
using std::string;
using std::stringstream;
using std::tie;
using std::tuple;
using std::unique_ptr;
using std::variant;
using std::vector;

template <typename T>
string toString(const T& obj) {
    stringstream ss;
    ss << obj;
    return ss.str();
}

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

}
