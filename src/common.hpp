#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "include/base/cef_logging.h"
#include "include/wrapper/cef_helpers.h"

using std::array;
using std::atomic;
using std::binary_search;
using std::cerr;
using std::cout;
using std::declval;
using std::enable_shared_from_this;
using std::fill;
using std::forward;
using std::function;
using std::future;
using std::ifstream;
using std::make_shared;
using std::make_unique;
using std::map;
using std::max;
using std::memory_order_relaxed;
using std::min;
using std::move;
using std::mt19937;
using std::ofstream;
using std::optional;
using std::ostream;
using std::pair;
using std::promise;
using std::queue;
using std::random_device;
using std::regex;
using std::regex_match;
using std::remove_const;
using std::remove_reference;
using std::set;
using std::shared_ptr;
using std::smatch;
using std::sort;
using std::string;
using std::stringstream;
using std::swap;
using std::thread;
using std::tie;
using std::tuple;
using std::uniform_int_distribution;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

using std::this_thread::sleep_for;

#ifdef NDEBUG
#define SHARED_ONLY_CLASS_LEAK_CHECK(ClassName)
#else
#define SHARED_ONLY_CLASS_LEAK_CHECK(ClassName) \
    constexpr static char leakCheckClassName_[] = #ClassName; \
    LeakCheckToken<ClassName, leakCheckClassName_> leakCheckToken_;

template <typename T, char const* Name>
class LeakCheckToken;

template <typename T, char const* Name>
class LeakChecker {
private:
    LeakChecker() : objectCount_(0) {}
    ~LeakChecker() {
        size_t leakCount = objectCount_.load();
        if(leakCount) {
            LOG(ERROR) << "MEMORY LEAK: " << leakCount << " " << Name << " objects remaining";
            CHECK(false);
        }
    }
    atomic<size_t> objectCount_;

    static LeakChecker<T, Name> instance_;

    friend class LeakCheckToken<T, Name>;
};

template <typename T, char const* Name>
LeakChecker<T, Name> LeakChecker<T, Name>::instance_;

template <typename T, char const* Name>
class LeakCheckToken {
public:
    LeakCheckToken() {
        LeakChecker<T, Name>::instance_.objectCount_.fetch_add(1, memory_order_relaxed);
    }
    ~LeakCheckToken() {
        LeakChecker<T, Name>::instance_.objectCount_.fetch_sub(1, memory_order_relaxed);
    }
};
#endif

#define DISABLE_COPY_MOVE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName& operator=(ClassName&&) = delete

#define SHARED_ONLY_CLASS(ClassName) \
    private: \
        SHARED_ONLY_CLASS_LEAK_CHECK(ClassName) \
        struct CKey {}; \
        template <typename T> \
        void afterConstruct_(T) {} \
    public: \
        template <typename... T> \
        static shared_ptr<ClassName> create(T&&... args) { \
            shared_ptr<ClassName> ret = make_shared<ClassName>(CKey(), forward<T>(args)...); \
            ret->afterConstruct_(ret); \
            return ret; \
        } \
        DISABLE_COPY_MOVE(ClassName)

// Convenience functions for posting tasks to be run from the CEF UI thread
// loop. May be called from any thread.
void postTask(function<void()> func);

template <typename T, typename... Args>
void postTask(shared_ptr<T> ptr, void (T::*func)(Args...), Args... args) {
    postTask([ptr, func, args...]() {
        (ptr.get()->*func)(args...);
    });
}

template <typename T, typename... Args>
void postTask(weak_ptr<T> weakPtr, void (T::*func)(Args...), Args... args) {
    postTask([weakPtr, func, args...]() {
        if(shared_ptr<T> ptr = weakPtr.lock()) {
            (ptr.get()->*func)(args...);
        }
    });
}

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
optional<T> parseString(
    string::const_iterator begin,
    string::const_iterator end
) {
    return parseString<T>(string(begin, end));
}

template <typename T>
optional<T> parseString(
    const pair<string::const_iterator, string::const_iterator>& str
) {
    return parseString<T>(str.first, str.second);
}

template <typename T>
string toString(const T& obj) {
    stringstream ss;
    ss << obj;
    return ss.str();
}
