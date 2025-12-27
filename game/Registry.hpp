#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

namespace game {
    namespace Detail {
        struct TypeIdGenerator {
        public:
            TypeIdGenerator() = default;

            template<typename T>
            static size_t typeId() {
                static size_t typeId = s_nextTypeId++;
                return typeId;
            }

        private:
            inline static size_t s_nextTypeId = 0;
        };
    };// namespace Detail

    struct Registry
        : public moe::Meta::NonCopyable<Registry>,
          public moe::Meta::Singleton<Registry> {
    public:
        MOE_SINGLETON(Registry)

        template<typename T, typename... Args>
        void emplace(Args&&... args) {
            size_t typeId = Detail::TypeIdGenerator::typeId<T>();
            if (m_registryMap.find(typeId) != m_registryMap.end()) {
                moe::Logger::warn(
                        "Registry::emplace: type already registered, overwriting");
            }

            m_registryMap[typeId] =
                    moe::UniquePtr<Wrapper>(new WrapperImpl<T>(std::forward<Args>(args)...));
        }

        template<typename T>
        T* get() {
            // this will fail directly if in debug mode
            size_t typeId = Detail::TypeIdGenerator::typeId<T>();
            auto it = m_registryMap.find(typeId);
            if (it == m_registryMap.end()) {
                MOE_ASSERT(
                        false,
                        "Registry::get: type not registered");
                return nullptr;
            }

            auto* wrapperImpl = static_cast<WrapperImpl<T>*>(it->second.get());
            if (!wrapperImpl) {
                MOE_ASSERT(
                        false,
                        "Registry::get: type mismatch for registered type");
                moe::Logger::error("Registry::get: type mismatch for registered type");
                return nullptr;
            }

            return &wrapperImpl->value;
        }

        template<typename T>
        bool contains() {
            size_t typeId = Detail::TypeIdGenerator::typeId<T>();
            return m_registryMap.find(typeId) != m_registryMap.end();
        }

        template<typename T>
        void remove() {
            size_t typeId = Detail::TypeIdGenerator::typeId<T>();
            m_registryMap.erase(typeId);
        }

        void clear() {
            m_registryMap.clear();
        }

    private:
        struct Wrapper {
            virtual ~Wrapper() = default;
        };

        template<typename T>
        struct WrapperImpl : public Wrapper {
            T value;

            template<typename... Args>
            explicit WrapperImpl(Args&&... args)
                : value(std::forward<Args>(args)...) {}
        };

        moe::UnorderedMap<size_t, moe::UniquePtr<Wrapper>> m_registryMap;
    };
}// namespace game