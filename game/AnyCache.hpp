#pragma once

#include "Core/Meta/Feature.hpp"
#include "Core/Meta/Generator.hpp"
#include "Core/Meta/TypeTraits.hpp"

namespace game {
    struct CacheKey {
        moe::String key;
        size_t hash;

        moe::String toString() const {
            return fmt::format("CacheKey{{ key: {}, hash: {} }}", key, hash);
        }
    };

    struct CacheKeyHasher {
        size_t operator()(const CacheKey& k) const {
            return k.hash;
        }
    };

    struct CacheKeyEqual {
        bool operator()(const CacheKey& a, const CacheKey& b) const {
            return a.hash == b.hash && a.key == b.key;
        }
    };

    template<typename T>
    struct AnyCache : moe::Meta::Singleton<AnyCache<T>> {
    public:
        MOE_SINGLETON(AnyCache<T>)

        void put(const CacheKey& cacheKey, const T& value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cache[cacheKey] = value;
        }

        moe::Optional<T> get(const CacheKey& cacheKey) {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_cache.find(cacheKey);
            if (it != m_cache.end()) {
                moe::Logger::debug("Cache hit for key: {}", cacheKey.toString());
                return it->second;
            }

            moe::Logger::debug("Cache miss for key: {}", cacheKey.toString());
            return std::nullopt;
        }

    private:
        std::mutex m_mutex;
        moe::UnorderedMap<CacheKey, T, CacheKeyHasher, CacheKeyEqual> m_cache;
    };

    template<
            typename InnerGenerator,
            typename = moe::Meta::EnableIfT<moe::Meta::IsGeneratorV<InnerGenerator>>>
    struct AnyCacheLoader {
    public:
        using value_type = typename InnerGenerator::value_type;
        using cache_type = AnyCache<value_type>;

        template<typename... Args>
        AnyCacheLoader(Args&&... args)
            : m_derived(std::forward<Args>(args)...) {
        }

        moe::Optional<value_type> generate() {
            CacheKey key = {
                    .key = m_derived.paramString(),
                    .hash = m_derived.hashCode(),
            };

            auto cachedValue = AnyCache<value_type>::getInstance().get(key);
            if (cachedValue.has_value()) {
                return cachedValue;
            }

            auto generatedValue = m_derived.generate();
            if (generatedValue.has_value()) {
                AnyCache<value_type>::getInstance().put(key, *generatedValue);
            }
            return generatedValue;
        }

        moe::String paramString() const {
            return m_derived.paramString();
        }

        uint64_t hashCode() const {
            return m_derived.hashCode();
        }

    private:
        InnerGenerator m_derived;
    };
}// namespace game