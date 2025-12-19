#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Generator.hpp"

MOE_BEGIN_NAMESPACE

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<Meta::IsGeneratorV<InnerGenerator>>>
struct Lazy {
public:
    using value_type = typename InnerGenerator::value_type;

    template<typename... Args>
    Lazy(Args&&... args)
        : m_derived(std::forward<Args>(args)...) {}

    Optional<value_type> generate() {
        std::call_once(m_initFlag, [this]() {
            m_cachedValue = m_derived.generate();
        });

        if (m_cachedValue) {
            return *m_cachedValue;
        }

        return std::nullopt;
    }

    uint64_t hashCode() const {
        return m_derived.hashCode();
    }

    String paramString() const {
        return m_derived.paramString();
    }

private:
    InnerGenerator m_derived;
    mutable Optional<value_type> m_cachedValue;
    mutable std::once_flag m_initFlag;
};

MOE_END_NAMESPACE