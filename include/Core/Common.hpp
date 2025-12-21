#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>


#include <tcb/span.hpp>

namespace moe {
    template<typename... TArgs>
    using UniquePtr = std::unique_ptr<TArgs...>;

    template<typename... TArgs>
    using SharedPtr = std::shared_ptr<TArgs...>;

    using String = std::string;

    using StringView = std::string_view;

    using U32String = std::u32string;

    using U32StringView = std::u32string_view;

    template<typename T, size_t N>
    using Array = std::array<T, N>;

    template<typename... TArgs>
    using Vector = std::vector<TArgs...>;

    template<typename... TArgs>
    using Queue = std::queue<TArgs...>;

    template<typename... TArgs>
    using Deque = std::deque<TArgs...>;

    template<typename T>
    using Optional = std::optional<T>;

    template<typename... TArgs>
    using Function = std::function<TArgs...>;

    template<typename T>
    using Span = tcb::span<T>;

    template<typename... TArgs>
    using Variant = std::variant<TArgs...>;

    template<typename... TArgs>
    using UnorderedMap = std::unordered_map<TArgs...>;

    template<typename... TArgs>
    using UnorderedSet = std::unordered_set<TArgs...>;

    template<typename FirstT, typename SecondT>
    using Pair = std::pair<FirstT, SecondT>;

    template<typename T>
    struct Pinned {
    private:
        UniquePtr<T> m_ptr;

    public:
        template<typename... Args>
        explicit Pinned(Args&&... args)
            : m_ptr(std::make_unique<T>(std::forward<Args>(args)...)) {}

        explicit Pinned(std::nullptr_t) : m_ptr(nullptr) {}

        T* operator->() { return m_ptr.get(); }

        const T* operator->() const { return m_ptr.get(); }

        T& operator*() { return *m_ptr; }

        const T& operator*() const { return *m_ptr; }

        T* get() { return m_ptr.get(); }

        const T* get() const { return m_ptr.get(); }

        Pinned(Pinned&& other) noexcept
            : m_ptr(std::move(other.m_ptr)) {}

        Pinned& operator=(Pinned&& other) noexcept {
            if (this != &other) {
                m_ptr = std::move(other.m_ptr);
            }
            return *this;
        }

        Pinned(const Pinned&) = delete;
        Pinned& operator=(const Pinned&) = delete;
    };

    template<typename T, typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
    Pinned<T> makePinned(Args&&... args) {
        return Pinned<T>(std::forward<Args>(args)...);
    }
}// namespace moe

#include "Core/Logger.hpp"

#define MOE_ASSERT(_cond, _msg) (assert(_cond&& _msg))

#define MOE_LOG_AND_THROW(_msg) \
    Logger::critical(_msg);     \
    throw std::runtime_error(_msg);

#define MOE_VK_CHECK_MSG(expr, msg)                     \
    do {                                                \
        VkResult result = expr;                         \
        if (result != VK_SUCCESS) {                     \
            Logger::critical(msg);                      \
            throw std::runtime_error(std::string(msg)); \
        }                                               \
    } while (false)

#define MOE_BEGIN_NAMESPACE namespace moe {
#define MOE_END_NAMESPACE }

namespace moe {
    // currently has no functionality
    inline String asset(const StringView path) {
        return String(path);
    }

    inline String userdata(const StringView path) {
        return String(path);
    }
}// namespace moe
