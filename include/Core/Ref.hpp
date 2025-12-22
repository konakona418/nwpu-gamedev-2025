#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/RefCounted.hpp"
#include "Core/Meta/TypeTraits.hpp"


MOE_BEGIN_NAMESPACE

template<typename T, typename = Meta::EnableIfT<(Meta::IsRefCountedV<T> || !Meta::IsCompleteTypeV<T>)>>
struct Ref {
public:
    template<typename V, typename>
    friend struct Ref;

    Ref() = default;

    explicit Ref(T* ptr)
        : m_ptr(ptr) {
        if (m_ptr) {
            retain();
        }
    }

    static Ref<T> null() {
        return Ref<T>(nullptr);
    }

    Ref(const Ref& other)
        : m_ptr(other.m_ptr) {
        if (m_ptr) {
            retain();
        }
    }

    template<typename U>
    Ref(const Ref<U>& other)
        : m_ptr(other.m_ptr) {
        static_assert(
                Meta::IsBaseOfV<T, U> || Meta::IsSameV<T, U>,
                "Cannot construct Ref<T> from Ref<U> when T is not a base of U, "
                "nor the same type");
        if (m_ptr) {
            retain();
        }
    }

    template<typename U>
    Ref<T>& operator=(const Ref<U>& other) {
        static_assert(
                Meta::IsBaseOfV<T, U> || Meta::IsSameV<T, U>,
                "Cannot assign Ref<T> from Ref<U> when T is not a base of U, "
                "nor the same type");
        if (m_ptr != other.m_ptr) {
            release();
            m_ptr = other.m_ptr;
            retain();
        }
        return *this;
    }

    template<typename U>
    Ref(Ref<U>&& other) noexcept
        : m_ptr(other.m_ptr) {
        static_assert(
                Meta::IsBaseOfV<T, U> || Meta::IsSameV<T, U>,
                "Cannot construct Ref<T> from Ref<U> when T is not a base of U, "
                "nor the same type");
        other.m_ptr = nullptr;
    }

    template<typename U>
    Ref& operator=(Ref<U>&& other) noexcept {
        static_assert(
                Meta::IsBaseOfV<T, U> || Meta::IsSameV<T, U>,
                "Cannot assign Ref<T> from Ref<U> when T is not a base of U, "
                "nor the same type");
        if (m_ptr == other.m_ptr) {
            other.m_ptr = nullptr;
            return *this;
        }

        release();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    Ref(Ref&& other) noexcept
        : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ~Ref() {
        release();
    }

    Ref& operator=(const Ref& other) {
        if (this != &other) {
            release();
            m_ptr = other.m_ptr;
            retain();
        }
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept {
        if (m_ptr == other.m_ptr) {
            other.m_ptr = nullptr;
            return *this;
        }
        release();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    T* get() {
        MOE_ASSERT(m_ptr != nullptr, "dereferencing null Ref pointer");
        return m_ptr;
    }

    const T* get() const {
        MOE_ASSERT(m_ptr != nullptr, "dereferencing null Ref pointer");
        return m_ptr;
    }

    T* operator->() {
        MOE_ASSERT(m_ptr != nullptr, "dereferencing null Ref pointer");
        return m_ptr;
    }

    const T* operator->() const {
        MOE_ASSERT(m_ptr != nullptr, "dereferencing null Ref pointer");
        return m_ptr;
    }

    bool operator==(const Ref& other) const { return m_ptr == other.m_ptr; }
    bool operator!=(const Ref& other) const { return m_ptr != other.m_ptr; }

    void reset(T* ptr = nullptr) {
        if (m_ptr != ptr) {
            release();
            m_ptr = ptr;
            retain();
        }
    }

    void swap(Ref& other) noexcept {
        std::swap(m_ptr, other.m_ptr);
    }

    explicit operator bool() const { return m_ptr != nullptr; }

private:
    void retain() {
        if (m_ptr) {
            m_ptr->retain();
        }
    }

    void release() {
        if (m_ptr) {
            m_ptr->release();
            m_ptr = nullptr;
        }
    }

    T* m_ptr{nullptr};
};

template<
        typename T,
        typename... Args,
        typename = Meta::EnableIfT<
                Meta::ConjunctionV<
                        Meta::IsRefCountedV<T>,
                        Meta::IsConstructibleV<T, Args...>>>>
Ref<T> makeRef(Args&&... args) {
    return Ref<T>(new T(std::forward<Args>(args)...));
}

MOE_END_NAMESPACE
