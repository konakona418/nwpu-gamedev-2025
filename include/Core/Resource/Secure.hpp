#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Generator.hpp"
#include "Core/Task/Scheduler.hpp"

MOE_BEGIN_NAMESPACE

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<Meta::IsGeneratorV<InnerGenerator>>>
struct Secure {
public:
    using value_type = typename InnerGenerator::value_type;

    template<typename... Args>
    Secure(Args&&... args)
        : m_derived(std::forward<Args>(args)...) {}

    Optional<value_type> generate() {
        if (m_cachedValue) {
            return *m_cachedValue;
        }

        if (!m_future.has_value()) {
            launchAsyncLoad();
            Logger::warn(
                    "Secure::generate() called but the async task was never initiated. "
                    "Blocking...");
        }

        // the job is created and executed directly on the main thread,
        // no need to wait for the future
        if (!m_executedOnMainThread) {
            MOE_ASSERT(m_future->isValid(),
                       "Future in Secure is not valid");

            m_future->get();
        }

        return m_cachedValue;
    }

    void launchAsyncLoad() {
        // if the current thread is the main thread, run directly
        if (MainScheduler::getInstance().isMainThread()) {
            Logger::debug("Secure::launchAsyncLoad running on main thread directly");
            m_cachedValue = m_derived.generate();
            m_executedOnMainThread = true;
            return;
        }

        m_future = asyncOnMainThread(
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
    Optional<Future<void, MainScheduler>> m_future;
    Optional<value_type> m_cachedValue;
    bool m_executedOnMainThread{false};
};

MOE_END_NAMESPACE