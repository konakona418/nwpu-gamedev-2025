#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

// triple buffer for data synchronization between threads
// strict
template<typename T>
struct TBuffer {
    static constexpr size_t SWAP_COUNT = 3;

    TBuffer() {
        m_writeBuffer = &m_buffers[0];
        m_pendingBuffer.store(&m_buffers[1], std::memory_order_relaxed);
        m_readBuffer = &m_buffers[2];
    }

    T& getWriteBuffer() {
        return *m_writeBuffer;
    }

    const T& getReadBuffer() const {
        return *m_readBuffer;
    }

    void publish() {
        m_writeBuffer = m_pendingBuffer.exchange(m_writeBuffer, std::memory_order_acq_rel);
        m_hasNewData.store(true, std::memory_order_release);
    }

    bool updateReadBuffer() {
        if (!m_hasNewData.load(std::memory_order_acquire)) {
            return false;
        }

        m_readBuffer = m_pendingBuffer.exchange(m_readBuffer, std::memory_order_acq_rel);
        m_hasNewData.store(false, std::memory_order_release);
        return true;
    }

private:
    T m_buffers[SWAP_COUNT];

    T* m_writeBuffer{nullptr};
    std::atomic<T*> m_pendingBuffer{nullptr};
    T* m_readBuffer{nullptr};

    std::atomic<bool> m_hasNewData{false};
};

MOE_END_NAMESPACE