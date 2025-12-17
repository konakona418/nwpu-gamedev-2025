#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

template<typename T>
struct Deferred {
public:
    Deferred() : m_isSet(false) {}

    void set(const T& value) {
        m_value = value;
        m_isSet.store(true, std::memory_order_release);
    }

    T get() const {
        MOE_ASSERT(m_isSet.load(std::memory_order_acquire), "Deferred value is not set");
        return m_value;
    }

    bool isSet() const {
        return m_isSet.load(std::memory_order_acquire);
    }

private:
    T m_value;
    std::atomic<bool> m_isSet;
};

MOE_END_NAMESPACE