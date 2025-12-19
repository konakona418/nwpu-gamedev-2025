#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Generator.hpp"
#include "Core/Task/Future.hpp"
#include "Core/Task/Scheduler.hpp"
#include "Core/Task/Utils.hpp"

MOE_BEGIN_NAMESPACE

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<Meta::IsGeneratorV<InnerGenerator>>>
struct Launch {
public:
    using value_type = typename InnerGenerator::value_type;

    template<typename... Args>
    Launch(Args&&... args)
        : m_derived(std::forward<Args>(args)...) {}

    Optional<value_type> generate() {
        if (m_cachedValue) {
            return *m_cachedValue;
        }

        if (!m_future.has_value()) {
            launchAsyncLoad();
            Logger::warn(
                    "Launch::generate() called but the async task was never initiated. "
                    "Blocking...");
        }

        MOE_ASSERT(m_future->isValid(),
                   "Future in AsyncLoad is not valid");

        m_future->get();
        return m_cachedValue;
    }

    void launchAsyncLoad() {
        m_future = async(
                [this]() {
                    Optional<value_type> value = m_derived.generate();
                    m_cachedValue = std::move(value);
                });
    }

    uint64_t hashCode() const {
        return m_derived.hashCode();
    }

    String paramString() const {
        return m_derived.paramString();
    }

private:
    InnerGenerator m_derived;
    Optional<Future<void, ThreadPoolScheduler>> m_future;
    Optional<value_type> m_cachedValue;
};

MOE_END_NAMESPACE
