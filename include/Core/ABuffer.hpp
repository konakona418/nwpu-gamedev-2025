#pragma once

#include "Core/Common.hpp"

MOE_BEGIN_NAMESPACE

/**
 * @brief Lock-free double buffer for concurrent access.
 *
 * ABuffer provides a lock-free double buffering mechanism suitable for scenarios
 * where a single writer thread updates data and one or more reader threads access
 * a consistent snapshot of the data. The writer thread should use getWriteBuffer()
 * to modify data, then call swap() to publish the new data to readers. Reader threads
 * should use getReadBuffer() to access the most recently published data.
 *
 * Thread-safety:
 * - Single writer: Only one thread should write to the buffer and call swap().
 * - Multiple readers: Any number of threads may concurrently call getReadBuffer().
 * - No locks are used; synchronization is achieved via atomic operations.
 *
 * Usage pattern:
 * - Writer: getWriteBuffer() -> modify data -> swap()
 * - Reader(s): getReadBuffer()
 */
template<typename T>
struct ABuffer {
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