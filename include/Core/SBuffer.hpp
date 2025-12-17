#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

template<typename T>
struct SBuffer {
public:
    void publish(T&& value) {
        auto p = std::make_shared<T>(std::forward<T>(value));
        std::atomic_store(&m_ptr, p);
    }

    T get() const {
        return *std::atomic_load(&m_ptr);
    }

private:
    SharedPtr<T> m_ptr = std::make_shared<T>();
};

MOE_END_NAMESPACE