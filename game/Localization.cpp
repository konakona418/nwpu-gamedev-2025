#include "Localization.hpp"

#include <fstream>
#include <utf8.h>

namespace game {
    void LocalizationParamManager::loadFromFile(const moe::StringView filepath) {
        m_filepath = filepath.data();

        auto table_ = toml::parse_file(filepath.data());
        if (table_.failed()) {
            moe::Logger::error(
                    "LocalizationParamManager::loadFromFile: failed to parse localization file {}: {}",
                    filepath,
                    table_.error().description());
            return;
        }

        m_table = table_.table();

        initAllParamsFromTable();
    }

    void LocalizationParamManager::saveToFile() const {
        if (!m_isDevMode) {
            moe::Logger::warn("LocalizationParamManager::saveToFile: not in dev mode; aborting");
            return;
        }

        if (m_filepath.empty()) {
            moe::Logger::error("LocalizationParamManager::saveToFile: filepath not set; aborting");
            return;
        }

        toml::table table;

        for (const auto& [name, paramPtr]: m_localizationMap) {
            const LocalizationItem& param = *paramPtr;
            const auto splitNames = splitPath(name);

            auto value = param.value;
            toml::table* currentTable = &table;
            for (size_t i = 0; i < splitNames.size() - 1; ++i) {
                const auto& part = splitNames[i];
                if (!currentTable->contains(part.data())) {
                    currentTable->insert(part.data(), toml::table{});
                }
                currentTable = (*currentTable)[part.data()].as_table();
            }
            const auto& lastPart = splitNames.back();
            currentTable->insert(lastPart.data(), utf8::utf32to8(value));
        }

        // ! fixme: stop using ofstream directly
        std::ofstream ofs(m_filepath.data());
        ofs << table;
    }

    void LocalizationParamManager::initAllParamsFromTable() {
        for (auto& [name, i18n]: m_localizationMap) {
            auto path = toml::path(name.data());
            if (path.empty()) {
                moe::Logger::warn(
                        "LocalizationParamManager::registerParam: invalid i18n key name: {}",
                        name);
                return;
            }

            auto valueInFile = m_table.at_path(path);
            if (!valueInFile) {
                return;
            }

            valueInFile.visit([i18n = i18n, name = std::ref(name)](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (toml::is_string<T>) {
                    i18n->value = utf8::utf8to32(arg.template as<typename T::value_type>()->get());
                } else {
                    moe::Logger::warn(
                            "LocalizationParamManager::registerParam: unsupported value type for i18n key {}",
                            name.get());
                    return;
                }
            });
        }
    }

    moe::Vector<moe::String> LocalizationParamManager::splitPath(const moe::StringView path) {
        moe::Vector<moe::String> result;
        size_t start = 0;
        size_t end = 0;
        while ((end = path.find('.', start)) != moe::StringView::npos) {
            result.push_back(moe::String(path.substr(start, end - start)));
            start = end + 1;
        }
        result.push_back(moe::String(path.substr(start)));
        return result;
    }
}// namespace game