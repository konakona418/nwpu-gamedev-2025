#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

template<typename T>
struct ABuffer {
public:
    using value_type = T;
    static constexpr size_t SWAP_COUNT = 2;

    ABuffer() {
        m_currentRead.store(&m_buffers[0], std::memory_order_relaxed);
        m_currentWrite = &m_buffers[1];
    }

    T& getWriteBuffer() {
        return *m_currentWrite;
    }

    const T& getReadBuffer() const {
        return *m_currentRead.load(std::memory_order_acquire);
    }

    T& getWriteBufferUnsafe() {
        return *m_currentWrite;
    }

    void swap() {
        T* newRead = m_currentWrite;
        m_currentWrite = m_currentRead.exchange(newRead, std::memory_order_release);
    }

private:
    Array<T, SWAP_COUNT> m_buffers;

    std::atomic<T*> m_currentRead{nullptr};
    T* m_currentWrite{nullptr};
};


MOE_END_NAMESPACE