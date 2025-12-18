#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

// atomic buffer for simple data synchronization between threads
// T is required to be trivially copyable
template<typename T>
struct ABuffer {
public:
    void publish(const T& value) {
        m_data.store(value, std::memory_order_release);
    }

    T get() const {
        return m_data.load(std::memory_order_acquire);
    }

private:
    std::atomic<T> m_data;
};

MOE_END_NAMESPACE