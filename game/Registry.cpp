#include "Registry.hpp"

#include <map>

namespace game::Detail::_TypeUtil {
    std::string _remove_redundant_space(const std::string& full_string) {
        std::string result = full_string;
        while (result.find(' ') != std::string::npos) {
            result.erase(result.find(' '), 1);
        }
        return result;
    }

    bool string_contains(const std::string& full_string, const std::string& sub_string) {
        return full_string.find(sub_string) != std::string::npos;
    }

    std::string _string_replace(
            std::string& full_string, const std::string& sub_string,
            const std::string& replacement) {
        return full_string.replace(full_string.find(sub_string), sub_string.length(), replacement);
    }

    std::string _extract_type(std::string& full_string) {
        size_t begin = full_string.find("[with T = ") + 9;
        size_t end = full_string.find(']');

        std::vector<size_t> semicolons;
        for (size_t i = begin; i < end; ++i) {
            if (full_string[i] == ';') {
                semicolons.push_back(i);
            }
        }

        std::vector<std::string> segments;
        for (size_t i = 0; i < semicolons.size(); ++i) {
            size_t start = i == 0 ? begin : semicolons[i - 1] + 1;
            size_t stop = semicolons[i];
            segments.push_back(_remove_redundant_space(full_string.substr(start, stop - start)));
        }
        segments.push_back(
                _remove_redundant_space(full_string.substr(semicolons.back() + 1, end - semicolons.back() - 1)));

        std::string base_type = _remove_redundant_space(segments[0]);
        std::map<std::string, std::string> aliases;
        aliases["T"] = base_type;
        for (size_t i = 1; i < segments.size(); ++i) {
            size_t equal_sign = segments[i].find('=');
            std::string alias = _remove_redundant_space(segments[i].substr(0, equal_sign));
            std::string type = _remove_redundant_space(segments[i].substr(equal_sign + 1));
            aliases[alias] = type;
        }


        for (auto it = aliases.rbegin(); it != aliases.rend(); ++it) {
            for (auto it2 = aliases.rbegin(); it2 != aliases.rend(); ++it2) {
                if (it == it2) continue;
                if (string_contains(it2->second, it->second)) {
                    _string_replace(it2->second, it->second, it->first);
                }
            }
        }
        return aliases["T"];
    }
}// namespace game::Detail::_TypeUtil