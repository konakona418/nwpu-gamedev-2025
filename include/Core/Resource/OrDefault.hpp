#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Generator.hpp"

MOE_BEGIN_NAMESPACE

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<Meta::IsGeneratorV<InnerGenerator>>>

struct OrDefault {
public:
    using value_type = typename InnerGenerator::value_type;

    template<typename... Args>
    OrDefault(value_type defaultValue, Args&&... args)
        : m_defaultValue(defaultValue), m_derived(std::forward<Args>(args)...) {}

    value_type generate() {
        Optional<value_type> generatedValue = m_derived.generate();
        if (generatedValue) {
            return *generatedValue;
        }

        return m_defaultValue;
    }

    uint64_t hashCode() const {
        return m_derived.hashCode();
    }

    String paramString() const {
        return m_derived.paramString();
    }

private:
    InnerGenerator m_derived;
    value_type m_defaultValue;
};

MOE_END_NAMESPACE