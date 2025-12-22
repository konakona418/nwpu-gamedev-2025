#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

#include "TomlImpl.hpp"

namespace game {
    struct LocalizationItem {
    public:
        moe::String key;
        moe::U32String value;
    };

    struct LocalizationParamManager
        : moe::Meta::NonCopyable<LocalizationParamManager>,
          moe::Meta::Singleton<LocalizationParamManager> {
    public:
        MOE_SINGLETON(LocalizationParamManager)

        void loadFromFile(const moe::StringView filepath);
        void saveToFile() const;

        void registerItem(LocalizationItem& item) {
            m_localizationMap[item.key] = &item;
            m_sortedKeys.push_back(item.key);
        }

        const moe::UnorderedMap<moe::String, LocalizationItem*>& getAllLocalizationItems() const {
            return m_localizationMap;
        }

        const moe::Vector<moe::String>& getSortedKeys() const {
            return m_sortedKeys;
        }

        void setDevMode(bool isDevMode) {
            m_isDevMode = isDevMode;
        }

        bool isDevMode() const {
            return m_isDevMode;
        }

    private:
        moe::UnorderedMap<moe::String, LocalizationItem*> m_localizationMap;
        moe::Vector<moe::String> m_sortedKeys;

        moe::String m_filepath;
        toml::table m_table;

        bool m_isDevMode{false};

        void initAllParamsFromTable();
        static moe::Vector<moe::String> splitPath(const moe::StringView path);
    };

    struct LocalizationKey {
    public:
        LocalizationKey(const moe::StringView key, const moe::U32String& defaultValue = U"") {
            m_item.key = key.data();
            m_item.value = defaultValue;
            LocalizationParamManager::getInstance().registerItem(m_item);
        }

        const moe::U32String& get() const {
            return m_item.value;
        }

    private:
        LocalizationItem m_item{};
    };

    using I18N = LocalizationKey;
}// namespace game