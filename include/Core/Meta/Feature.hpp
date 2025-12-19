#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

namespace Meta {
    template<typename Derived>
    struct NonCopyable {
    public:
        NonCopyable() = default;
        ~NonCopyable() = default;

        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;

        NonCopyable(NonCopyable&&) = default;
        NonCopyable& operator=(NonCopyable&&) = default;
    };

    template<typename Derived>
    struct Singleton {
    public:
        static Derived& getInstance() {
            static Derived s_instance;
            return s_instance;
        }

    protected:
        Singleton() = default;
        ~Singleton() = default;

        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;

        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton&&) = delete;
    };

#define MOE_SINGLETON(_ClassName_) \
    friend struct ::moe::Meta::Singleton<_ClassName_>;
}// namespace Meta

MOE_END_NAMESPACE