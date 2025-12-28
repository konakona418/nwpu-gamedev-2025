#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"
#include "Core/Ref.hpp"

namespace game {
    namespace Detail {
        namespace _TypeUtil {
            // from my previous project
            // https://github.com/konakona418/my-reflection/blob/master/include/simple_refl.h

            std::string _remove_redundant_space(const std::string& full_string);
            bool string_contains(const std::string& full_string, const std::string& sub_string);
            std::string _string_replace(
                    std::string& full_string, const std::string& sub_string,
                    const std::string& replacement);
            std::string _extract_type(std::string& full_string);

            template<typename T>
            std::string extract_type_name(bool& success) {
                std::string fn_sig;
                std::string type_name;
#if defined(__GNUC__) || defined(__clang__)
                fn_sig = __PRETTY_FUNCTION__;
                type_name = _extract_type(fn_sig);
                success = true;
#else
                success = false;
                return "<Unknown Type; Compiler Unsupported>";
#endif
                return type_name;
            }
        }// namespace _TypeUtil

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

#ifndef NDEBUG
            // show type name for debugging
            {
                bool success = false;
                auto typeName =
                        Detail::_TypeUtil::extract_type_name<T>(success);
                if (success) {
                    moe::Logger::debug(
                            "TypeIdGenerator: registered type '{}' with type ID {}",
                            typeName, typeId);
                } else {
                    moe::Logger::warn(
                            "TypeIdGenerator: registered type with type ID {}, but failed to extract type name",
                            typeId);
                }
            }
#endif

            m_registryMap[typeId] =
                    moe::UniquePtr<Wrapper>(new WrapperImpl<T>(std::forward<Args>(args)...));
        }

        template<typename T>
        moe::Ref<T> get() {
            // this will fail directly if in debug mode
            size_t typeId = Detail::TypeIdGenerator::typeId<T>();
            auto it = m_registryMap.find(typeId);
            if (it == m_registryMap.end()) {
                MOE_ASSERT(
                        false,
                        "Registry::get: type not registered");
                return moe::Ref<T>(nullptr);
            }

            auto* wrapperImpl = static_cast<WrapperImpl<T>*>(it->second.get());
            if (!wrapperImpl) {
                MOE_ASSERT(
                        false,
                        "Registry::get: type mismatch for registered type");
                moe::Logger::error("Registry::get: type mismatch for registered type");
                return moe::Ref<T>(nullptr);
            }

            return wrapperImpl->value;
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
            moe::Ref<T> value;

            template<typename... Args>
            explicit WrapperImpl(Args&&... args)
                : value(moe::Ref(new T(std::forward<Args>(args)...))) {}
        };

        moe::UnorderedMap<size_t, moe::UniquePtr<Wrapper>> m_registryMap;
    };
}// namespace game