#pragma once

#include "Core/Common.hpp"
#include "Core/Ref.hpp"

MOE_BEGIN_NAMESPACE

typedef void (*RefCountedDeleterFn)(void*);

template<typename T>
struct RefCounted {
public:
    void retain() {
        ++m_refCount;
    }

    void release() {
        MOE_ASSERT(m_refCount > 0, "release called on object with zero ref count");
        --m_refCount;
        if (m_refCount != 0) {
            return;
        }

        if (m_deleter) {
            m_deleter(this);
            return;
        }
        delete static_cast<T*>(this);
    }

    size_t getRefCount() const {
        return m_refCount;
    }

    Ref<T> intoRef() {
        MOE_ASSERT(m_refCount > 0,
                   "intoRef called on object with zero ref count. "
                   "It should only be called on objects that are already Ref<T> managed.");
        return Ref<T>(static_cast<T*>(this));
    }

    void setDeleter(RefCountedDeleterFn deleter) {
        m_deleter = deleter;
    }

private:
    size_t m_refCount{0};
    RefCountedDeleterFn m_deleter{nullptr};
};

template<typename T>
struct AtomicRefCounted {
public:
    void retain() {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void release() {
        size_t oldCount = m_refCount.fetch_sub(1, std::memory_order_acq_rel);
        MOE_ASSERT(oldCount > 0, "release called on object with zero ref count");
        if (oldCount != 1) {
            return;
        }

        if (m_deleter) {
            m_deleter(this);
            return;
        }
        delete static_cast<T*>(this);
    }

    size_t getRefCount() const {
        return m_refCount.load(std::memory_order_relaxed);
    }

    Ref<T> intoRef() {
        MOE_ASSERT(m_refCount.load(std::memory_order_relaxed) > 0,
                   "intoRef called on object with zero ref count. "
                   "It should only be called on objects that are already Ref<T> managed.");
        return Ref<T>(static_cast<T*>(this));
    }

    template<
            typename U,
            typename = Meta::EnableIfT<
                    Meta::DisjunctionV<
                            Meta::IsBaseOfV<T, U>,
                            Meta::IsBaseOfV<U, T>,
                            Meta::IsSameV<T, U>>>>
    Ref<U> asRef() {
        MOE_ASSERT(m_refCount.load(std::memory_order_relaxed) > 0,
                   "intoRef called on object with zero ref count. "
                   "It should only be called on objects that are already Ref<T> managed.");
        return Ref<U>(static_cast<U*>(this));
    }

    void setDeleter(RefCountedDeleterFn deleter) {
        m_deleter = deleter;
    }

private:
    std::atomic<size_t> m_refCount{0};
    RefCountedDeleterFn m_deleter{nullptr};
};

MOE_END_NAMESPACE