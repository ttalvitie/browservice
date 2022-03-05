#pragma once

#ifdef _WIN32
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <climits>
#include <codecvt>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "include/base/cef_logging.h"
#include "include/wrapper/cef_helpers.h"

namespace browservice {

using std::array;
using std::atomic;
using std::binary_search;
using std::cerr;
using std::codecvt_utf8;
using std::cout;
using std::declval;
using std::enable_shared_from_this;
using std::exception;
using std::fill;
using std::forward;
using std::function;
using std::future;
using std::ifstream;
using std::lock_guard;
using std::make_shared;
using std::make_unique;
using std::map;
using std::max;
using std::memory_order_relaxed;
using std::min;
using std::move;
using std::mt19937;
using std::mutex;
using std::ofstream;
using std::optional;
using std::ostream;
using std::pair;
using std::promise;
using std::queue;
using std::random_device;
using std::range_error;
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
using std::tolower;
using std::tuple;
using std::uniform_int_distribution;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;
using std::wstring;
using std::wstringstream;
using std::wstring_convert;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

using std::this_thread::sleep_for;

extern thread_local mt19937 rng;

#ifdef _WIN32
typedef wstring PathStr;
const wchar_t PathSep = L'\\';
#define PATHSTR(x) L ## x
#else
typedef string PathStr;
const char PathSep = '/';
#define PATHSTR(x) x
#endif

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

template <typename T>
PathStr toPathStr(const T& obj) {
#ifdef _WIN32
    wstringstream ss;
#else
    stringstream ss;
#endif
    ss << obj;
    return ss.str();
}

string sanitizeUTF8String(string str);
vector<int> sanitizeUTF8StringToCodePoints(string str);

// Logging macros that log given message along with severity, source file and
// line information to stderr. Message is formed by calling toString for each
// argument and concatenating the results.
#define INFO_LOG LogWriter("INFO", __FILE__, __LINE__)
#define WARNING_LOG LogWriter("WARNING", __FILE__, __LINE__)
#define ERROR_LOG LogWriter("ERROR", __FILE__, __LINE__)

class LogWriter {
public:
    LogWriter(const char* severity, const char* file, int line)
        : LogWriter(severity, string(file) + ":" + toString(line))
    {}
    LogWriter(const char* severity, string location)
        : severity_(severity),
          location_(move(location))
    {}

    template <typename... T>
    void operator()(const T&... args) {
        vector<string> argStrs = {toString(args)...};
        stringstream msg;
        msg << severity_ << " @ " << location_ << " -- ";
        for(const string& argStr : argStrs) {
            msg << argStr;
        }
        msg << "\n";
        cerr << msg.str();
    }

private:
    const char* severity_;
    string location_;
};

// Panic and assertion macros for ending the program in the case of
// irrecoverable errors. By default the program exits using abort(), but after
// enablePanicUsingCEFFatalError has been called, the program is exited using
// the CEF logging system, by invoking LOG(FATAL).
#define PANIC Panicker(__FILE__, __LINE__)
#define REQUIRE(cond) \
    do { if(!(cond)) { PANIC("Requirement '" #cond "' failed"); } } while(false)

class Panicker {
public:
    Panicker(const char* file, int line)
        : Panicker(string(file) + ":" + toString(line))
    {}
    Panicker(string location)
        : location_(move(location))
    {}
    ~Panicker() {
        panic_("");
    }

    template <typename... T>
    void operator()(const T&... args) {
        vector<string> argStrs = {toString(args)...};
        string msg;
        for(const string& argStr : argStrs) {
            msg.append(argStr);
        }
        panic_(move(msg));
    }

private:
    void panic_(string msg);

    string location_;
};

// Should only be called after CefInitialize has been successfully run.
void enablePanicUsingCEFFatalError();

// Boilerplate for defining classes that can only be constructed into
// shared_ptrs using the 'create' static function and which are checked for
// leaks at the end of the program on debug builds.
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
            PANIC("MEMORY LEAK: ", leakCount, " ", Name, " objects remaining");
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

// The macro REQUIRE_UI_THREAD is a version of CEF_REQUIRE_UI_THREAD that is
// allowed to be called in any thread unless specifically enabled by
// setRequireUIThreadEnabled. It should be enabled only when the control is in
// the CEF event loop.
#ifdef NDEBUG
    inline void setRequireUIThreadEnabled(bool value) {}
    #define REQUIRE_UI_THREAD() do {} while(false)
#else
    extern atomic<bool> requireUIThreadEnabled_;

    inline void setRequireUIThreadEnabled(bool value) {
        requireUIThreadEnabled_.store(value);
    }

    #define REQUIRE_UI_THREAD() \
        do { \
            if(requireUIThreadEnabled_.load()) { \
                CEF_REQUIRE_UI_THREAD(); \
            } \
        } while(false)
#endif

}
