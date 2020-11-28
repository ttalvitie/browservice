#pragma once

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace retrojsvice {

using std::atomic;
using std::cerr;
using std::exception;
using std::forward;
using std::function;
using std::get;
using std::get_if;
using std::lock_guard;
using std::make_shared;
using std::memory_order_relaxed;
using std::mutex;
using std::pair;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::variant;
using std::vector;

template <typename T>
string toString(const T& obj) {
    stringstream ss;
    ss << obj;
    return ss.str();
}

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

}
