#pragma once

#include "Core/Common.hpp"

#include "Core/Meta/Feature.hpp"
#include "Core/Meta/TypeTraits.hpp"

#include "TomlImpl.hpp"

namespace game {

    using ParamFloat = float;
    using ParamInt = int64_t;
    using ParamBool = bool;
    using ParamString = moe::String;

    using ParamValue = moe::Variant<ParamInt, ParamFloat, ParamBool, ParamString>;

    enum class ParamType {
        Int,
        Float,
        Bool,
        String
    };

    struct ParamItem {
    public:
        moe::String name;
        ParamValue value;

        template<typename T>
        T get() const {
            return std::get<T>(value);
        }

        ParamType getType() const;
    };

    struct ParamManager
        : moe::Meta::NonCopyable<ParamManager>,
          moe::Meta::Singleton<ParamManager> {
    public:
        MOE_SINGLETON(ParamManager)

        void loadFromFile(const moe::StringView filepath);

        void saveToFile(const moe::StringView filepath) const;

        void registerParam(const moe::StringView name, ParamItem& param);

        const moe::UnorderedMap<moe::String, ParamItem*>& getAllParams() const {
            return m_params;
        }

    private:
        moe::UnorderedMap<moe::String, ParamItem*> m_params;
        toml::table m_table;
        moe::String m_filepath;

        static moe::Vector<moe::String> splitPath(const moe::StringView path);

        void initAllParamsFromTable();
    };

    template<typename T>
    struct Param {
    public:
        static_assert(
                moe::Meta::IsSameV<T, ParamInt> ||
                        moe::Meta::IsSameV<T, ParamFloat> ||
                        moe::Meta::IsSameV<T, ParamBool> ||
                        moe::Meta::IsSameV<T, ParamString>,
                "Unsupported Param type");

        Param(const moe::StringView name, const T& defaultValue) {
            m_param = ParamItem{
                    .name = moe::String(name),
                    .value = defaultValue,
            };
            ParamManager::getInstance().registerParam(name, m_param);
        }

        T get() const {
            MOE_ASSERT(
                    std::holds_alternative<T>(m_param.value),
                    "Type mismatch when getting param");
            return m_param.get<T>();
        }

        void set(const T& value) {
            m_param.value = value;
        }

        operator T() const {
            return get();
        }

    private:
        ParamItem m_param;
    };

}// namespace game