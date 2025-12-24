#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Generator.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Secure.hpp"

MOE_BEGIN_NAMESPACE

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<Meta::IsGeneratorV<InnerGenerator>>>
struct Preload {
public:
    using value_type = typename InnerGenerator::value_type;

    template<typename... Args>
    Preload(Args&&... args)
        : m_derived(std::forward<Args>(args)...) {
        m_cachedValue = m_derived.generate();
    }

    Optional<value_type> generate() {
        if (m_cachedValue) {
            return *m_cachedValue;
        }

        return std::nullopt;
    }

    uint64_t hashCode() const {
        return m_derived->hashCode();
    }

    String paramString() const {
        return m_derived->paramString();
    }

private:
    InnerGenerator m_derived;
    Optional<value_type> m_cachedValue;
};

template<typename AsyncInnerGenerator>
struct Preload<Launch<AsyncInnerGenerator>> {
public:
    using value_type = typename AsyncInnerGenerator::value_type;

    template<typename... Args>
    Preload(Args&&... args) : m_asyncLoad(std::forward<Args>(args)...) {
        m_asyncLoad.launchAsyncLoad();
    }

    Optional<value_type> generate() {
        return m_asyncLoad.generate();
    }

    uint64_t hashCode() const {
        return m_asyncLoad.hashCode();
    }

    String paramString() const {
        return m_asyncLoad.paramString();
    }

private:
    Launch<AsyncInnerGenerator> m_asyncLoad;
};

template<typename SecureInnerGenerator>
struct Preload<Secure<SecureInnerGenerator>> {
public:
    using value_type = typename SecureInnerGenerator::value_type;

    template<typename... Args>
    Preload(Args&&... args) : m_secureLoad(std::forward<Args>(args)...) {
        m_secureLoad.launchAsyncLoad();
    }

    Optional<value_type> generate() {
        return m_secureLoad.generate();
    }

    uint64_t hashCode() const {
        return m_secureLoad.hashCode();
    }

    String paramString() const {
        return m_secureLoad.paramString();
    }

private:
    Secure<SecureInnerGenerator> m_secureLoad;
};

MOE_END_NAMESPACE