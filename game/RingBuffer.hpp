#pragma once

#include "Core/Common.hpp"

namespace game {
    template<typename T, size_t BufferSize>
    struct RingBuffer {
    public:
        static_assert((BufferSize & (BufferSize - 1)) == 0, "BufferSize must be a power of 2");

        static constexpr size_t bufferSize = BufferSize;

        void pushBack(const T& item) {
            m_buffer[m_nextIdx] = item;
            m_nextIdx = (m_nextIdx + 1) & (bufferSize - 1);
            if (m_size < bufferSize) m_size++;
        }

        T& get(size_t indexFromNewest) {
            size_t idx = (m_nextIdx + bufferSize - 1 - indexFromNewest) & (bufferSize - 1);
            return m_buffer[idx];
        }

        const T& get(size_t indexFromNewest) const {
            size_t idx = (m_nextIdx + bufferSize - 1 - indexFromNewest) & (bufferSize - 1);
            return m_buffer[idx];
        }

        T& operator[](size_t index) {
            return get(index);
        }

        const T& operator[](size_t index) const {
            return get(index);
        }

        size_t size() const {
            return m_size;
        }

        bool empty() const {
            return m_size == 0;
        }

        T& back() {
            return get(0);
        }

        const T& back() const {
            return get(0);
        }

        T& front() {
            return get(m_size - 1);
        }

        const T& front() const {
            return get(m_size - 1);
        }

    private:
        T m_buffer[bufferSize]{};
        size_t m_nextIdx{0};
        size_t m_size{0};
    };
}// namespace game