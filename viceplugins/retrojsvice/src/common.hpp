#pragma once

#ifdef _WIN32
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <codecvt>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace retrojsvice {

using std::atomic;
using std::cerr;
using std::codecvt_utf8;
using std::condition_variable;
using std::enable_shared_from_this;
using std::exception;
using std::forward;
using std::function;
using std::future;
using std::future_error;
using std::get;
using std::get_if;
using std::ifstream;
using std::istream;
using std::lock_guard;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::max;
using std::memory_order_relaxed;
using std::map;
using std::min;
using std::move;
using std::mt19937;
using std::multimap;
using std::mutex;
using std::numeric_limits;
using std::optional;
using std::ofstream;
using std::ostream;
using std::pair;
using std::promise;
using std::queue;
using std::random_device;
using std::range_error;
using std::seed_seq;
using std::set;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::swap;
using std::thread;
using std::tie;
using std::tuple;
using std::uniform_int_distribution;
using std::unique_lock;
using std::unique_ptr;
using std::variant;
using std::vector;
using std::visit;
using std::weak_ptr;
using std::wstring;
using std::wstringstream;
using std::wstring_convert;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

using std::this_thread::sleep_for;

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
string toString(const T& obj) {
    stringstream ss;
    ss << obj;
    return ss.str();
}

#ifdef _WIN32
static string toString(const wstring& obj) {
    stringstream ss;
    ss << obj.c_str();
    return ss.str();
}
#endif

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

vector<string> splitStr(
    const string& str,
    char delim,
    size_t maxSplits = numeric_limits<size_t>::max()
);

bool isNonEmptyNumericStr(const string& str);

// Helper class for defining visitors for variants
template<class... T> struct Overloaded : T... { using T::operator()...; };
template<class... T> Overloaded(T...) -> Overloaded<T...>;

// Logging macros that log given message along with log level, source file and
// line information to stderr. Message is formed by calling toString for each
// argument and concatenating the results.
#define INFO_LOG LogWriter(LogLevel::Info, __FILE__, __LINE__)
#define WARNING_LOG LogWriter(LogLevel::Warning, __FILE__, __LINE__)
#define ERROR_LOG LogWriter(LogLevel::Error, __FILE__, __LINE__)

enum class LogLevel {
    Info,
    Warning,
    Error
};

class LogWriter {
public:
    LogWriter(LogLevel logLevel, const char* file, int line)
        : LogWriter(logLevel, string(file) + ":" + toString(line))
    {}
    LogWriter(LogLevel logLevel, string location)
        : logLevel_(logLevel),
          location_(move(location))
    {}

    template <typename... T>
    void operator()(const T&... args) {
        vector<string> argStrs = {toString(args)...};
        string msg;
        for(const string& argStr : argStrs) {
            msg.append(argStr);
        }
        log_(move(msg));
    }

private:
    void log_(string msg);

    LogLevel logLevel_;
    string location_;
};

// Panic and assertion macros for ending the program in the case of
// irrecoverable errors.
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

// Set logging and panicking backends. Pass empty function to revert to default
// behavior.
void setLogCallback(function<void(LogLevel, const char*, const char*)> callback);
void setPanicCallback(function<void(const char*, const char*)> callback);

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
            cerr << "PANIC @ retrojsvice-plugin: MEMORY LEAK: ";
            cerr << leakCount << " " << Name << " objects remaining\n";
            cerr.flush();
            abort();
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

char* createMallocString(string val);

// We call the thread currently executing a plugin API call related to a context
// the "API thread". While it is not necessarily always the same thread, the
// plugin API guarantees that at most one API call for the same context is
// running at a time.
//
// Most of the plugin logic runs in the API thread (only blocking and CPU
// intensive parts are offloaded to background threads); threads can post tasks
// to be run in the API thread using postTask in task_queue.hpp.
//
// The macro REQUIRE_API_THREAD checks that the code is running in the API
// thread.
#define REQUIRE_API_THREAD() \
    do { \
        if(!inAPIThread_) { \
            PANIC("REQUIRE_API_THREAD failed"); \
        } \
    } while(false)

// This value should only be modified by Context (set to true when entering a
// plugin API function and to false when exiting the function).
extern thread_local bool inAPIThread_;

// Marker object used as first argument in class member functions to annotate
// that the function May Call Event handlers registered to the class directly.
// This makes sure that the caller is aware of this possibility, because the
// caller has to specify the argument explicitly. In addition, adding this
// argument retroactively to a function will ensure that the API compatibility
// breaks, ensuring that all the places where the function is called from are
// checked.
struct MCE {};
extern MCE mce;

}
