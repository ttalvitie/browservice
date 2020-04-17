#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include "include/base/cef_logging.h"
#include "include/wrapper/cef_helpers.h"

using std::atomic;
using std::enable_shared_from_this;
using std::forward;
using std::make_shared;
using std::memory_order_relaxed;
using std::shared_ptr;
using std::weak_ptr;

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

#define SHARED_ONLY_CLASS(ClassName) \
    private: \
        SHARED_ONLY_CLASS_LEAK_CHECK(ClassName) \
        struct CKey {}; \
    public: \
        template <typename... T> \
        static shared_ptr<ClassName> create(T&&... args) { \
            return make_shared<ClassName>(CKey(), forward<T>(args)...); \
        } \
        ClassName(const ClassName&) = delete; \
        ClassName(ClassName&&) = delete; \
        ClassName& operator=(const ClassName&) = delete; \
        ClassName& operator=(ClassName&&) = delete

// Convenience functions for posting tasks to be run from the CEF UI thread loop
void postTask(std::function<void()> func);

template <typename T, typename... Args>
void postTask(weak_ptr<T> weakPtr, void (T::*func)(Args...), Args... args) {
    postTask([weakPtr, func, args...]() {
        if(shared_ptr<T> ptr = weakPtr.lock()) {
            (ptr.get()->*func)();
        }
    });
}
