#include "Param.hpp"

#include <fstream>

namespace game {
    ParamType ParamItem::getType() const {
        if (std::holds_alternative<ParamInt>(value)) {
            return ParamType::Int;
        } else if (std::holds_alternative<ParamFloat>(value)) {
            return ParamType::Float;
        } else if (std::holds_alternative<ParamBool>(value)) {
            return ParamType::Bool;
        } else if (std::holds_alternative<ParamString>(value)) {
            return ParamType::String;
        } else {
            MOE_ASSERT(false, "Unknown ParamType");
            return ParamType::Int;// make linter happy
        }
    }

    void BaseParamManager::loadFromFile(const moe::StringView filepath) {
        m_filepath = filepath.data();

        auto table_ = toml::parse_file(filepath.data());
        if (table_.failed()) {
            moe::Logger::error(
                    "ParamManager::loadFromFile: failed to parse param file {}: {}",
                    filepath,
                    table_.error().description());
            return;
        }

        m_table = table_.table();

        initAllParamsFromTable();
    }

    void BaseParamManager::saveToFile() const {
        toml::table table;

        for (const auto& [name, paramPtr]: m_params) {
            const ParamItem& param = *paramPtr;
            const auto splitNames = splitPath(name);
            std::visit(
                    [name = std::ref(name), &table, &splitNames](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        toml::table* currentTable = &table;
                        for (size_t i = 0; i < splitNames.size() - 1; ++i) {
                            const auto& part = splitNames[i];
                            if (!currentTable->contains(part.data())) {
                                currentTable->insert(part.data(), toml::table{});
                            }
                            currentTable = (*currentTable)[part.data()].as_table();
                        }
                        const auto& lastPart = splitNames.back();
                        currentTable->insert(lastPart.data(), arg);
                    },
                    param.value);
        }

        // ! fixme: stop using ofstream directly
        std::ofstream ofs(m_filepath.data());
        ofs << table;
    }

    void BaseParamManager::registerParam(const moe::StringView name, ParamItem& param) {
        m_params.emplace(name.data(), &param);
    }

    void BaseParamManager::initAllParamsFromTable() {
        for (auto& [name, param]: m_params) {
            auto path = toml::path(name.data());
            if (path.empty()) {
                moe::Logger::warn(
                        "ParamManager::registerParam: invalid param name: {}",
                        name);
                return;
            }

            auto valueInFile = m_table.at_path(path);
            if (!valueInFile) {
                return;
            }

            valueInFile.visit([param = param, name = std::ref(name)](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (toml::is_integer<T> || toml::is_string<T> || toml::is_boolean<T>) {
                    param->value = arg.template as<typename T::value_type>()->get();
                } else if (toml::is_floating_point<T>) {
                    if constexpr (moe::Meta::IsSameV<float, ParamFloat>) {
                        // force cast to float
                        param->value = static_cast<float>(arg.template as<double>()->get());
                    } else {
                        param->value = arg.template as<typename T::value_type>()->get();
                    }
                } else {
                    moe::Logger::warn(
                            "ParamManager::registerParam: unsupported param type for param {}",
                            name.get());
                    return;
                }
            });
        }
    }

    moe::Vector<moe::String> BaseParamManager::splitPath(const moe::StringView path) {
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

    void ParamManager::saveToFile() const {
        if (!m_isDevMode) {
            moe::Logger::error("ParamManager::saveToFile: attempted to save params while not in dev mode; aborting");
            return;
        }
        BaseParamManager::saveToFile();
    }
}// namespace game