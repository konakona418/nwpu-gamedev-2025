#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

// atomic consumable flag value
struct AFlag {
public:
    AFlag() : m_flag(false) {}

    explicit AFlag(bool initialValue) : m_flag(initialValue) {}

    // set the flag
    void set() {
        m_flag.store(true, std::memory_order_release);
    }

    // consume the flag, return true if it was set
    bool consume() {
        return m_flag.exchange(false, std::memory_order_acq_rel);
    }

private:
    std::atomic<bool> m_flag{false};
};

MOE_END_NAMESPACE