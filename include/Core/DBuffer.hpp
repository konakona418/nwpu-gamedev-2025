#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/TypeTraits.hpp"

MOE_BEGIN_NAMESPACE

// double buffer container for data synchronization between threads
// but with more intuitive API than TBuffer
// relaxed than TBuffer
template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
struct DBuffer {
public:
    void publish(const T& value) {
        m_buffers[m_writingIdx] = value;
        m_frontIdx.store(m_writingIdx, std::memory_order_release);
        m_writingIdx = 1 - m_writingIdx;
    }

    T get() const {
        return m_buffers[m_frontIdx.load(std::memory_order_acquire)];
    }

private:
    std::atomic<int> m_frontIdx{0};
    T m_buffers[2];

    // producer exclusive
    int m_writingIdx{1};
};

MOE_END_NAMESPACE