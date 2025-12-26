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

    struct ParamFloat4 {
        float x;
        float y;
        float z;
        float w;
    };

    using ParamValue = moe::Variant<ParamInt, ParamFloat, ParamBool, ParamString, ParamFloat4>;

    enum class ParamType {
        Int,
        Float,
        Bool,
        String,
        Float4
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

    struct BaseParamManager {
    public:
        virtual ~BaseParamManager() = default;

        void loadFromFile(const moe::StringView filepath);

        virtual void saveToFile() const;

        void registerParam(const moe::StringView name, ParamItem& param);

        const moe::UnorderedMap<moe::String, ParamItem*>& getAllParams() const {
            return m_params;
        }

        const moe::Vector<moe::String>& getSortedParamNames() const {
            return m_sortedParams;
        }

    protected:
        moe::UnorderedMap<moe::String, ParamItem*> m_params;
        moe::Vector<moe::String> m_sortedParams;
        toml::table m_table;
        moe::String m_filepath;

        static moe::Vector<moe::String> splitPath(const moe::StringView path);

        void initAllParamsFromTable();
    };

    struct ParamManager
        : BaseParamManager,
          moe::Meta::NonCopyable<ParamManager>,
          moe::Meta::Singleton<ParamManager> {
    public:
        MOE_SINGLETON(ParamManager)

        void saveToFile() const override;

        void setDevMode(bool isDevMode) {
            m_isDevMode = isDevMode;
        }

        bool isDevMode() const {
            return m_isDevMode;
        }

    private:
        bool m_isDevMode{false};
    };

    struct UserConfigParamManager
        : BaseParamManager,
          moe::Meta::NonCopyable<UserConfigParamManager>,
          moe::Meta::Singleton<UserConfigParamManager> {
    public:
        MOE_SINGLETON(UserConfigParamManager)
    };

    enum class ParamScope {
        System,
        UserConfig,
    };

    template<typename T>
    struct Param {
    public:
        static_assert(
                moe::Meta::IsSameV<T, ParamInt> ||
                        moe::Meta::IsSameV<T, ParamFloat> ||
                        moe::Meta::IsSameV<T, ParamBool> ||
                        moe::Meta::IsSameV<T, ParamString> ||
                        moe::Meta::IsSameV<T, ParamFloat4>,
                "Unsupported Param type");

        Param(const moe::StringView name, const T& defaultValue, ParamScope scope = ParamScope::System) {
            m_param = ParamItem{
                    .name = moe::String(name),
                    .value = defaultValue,
            };
            switch (scope) {
                case ParamScope::System:
                    ParamManager::getInstance().registerParam(name, m_param);
                    break;
                case ParamScope::UserConfig:
                    UserConfigParamManager::getInstance().registerParam(name, m_param);
                    break;
            }
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

    using ParamI = Param<ParamInt>;
    using ParamF = Param<ParamFloat>;
    using ParamB = Param<ParamBool>;
    using ParamS = Param<ParamString>;
    using ParamF4 = Param<ParamFloat4>;

}// namespace game