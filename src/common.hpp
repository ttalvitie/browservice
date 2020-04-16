#pragma once

#include <memory>
#include <utility>

#define SHARED_ONLY_CLASS(ClassName) \
    public: \
        template <typename... T> \
        static std::shared_ptr<ClassName> create(T&&... args) { \
            return std::make_shared<ClassName>(CKey(), std::forward<T>(args)...); \
        } \
        ClassName(const ClassName&) = delete; \
        ClassName(ClassName&&) = delete; \
        ClassName& operator=(const ClassName&) = delete; \
        ClassName& operator=(ClassName&&) = delete; \
    private: \
        struct CKey {}
