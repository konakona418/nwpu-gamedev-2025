#include "Util.hpp"

namespace game::Util {
    moe::StringView glyphRangeChinese() {
        static moe::String s_glyphRange;
        static std::once_flag s_initFlag;

        std::call_once(s_initFlag, [range = std::ref(s_glyphRange)]() mutable {
            moe::Vector<char32_t> ss;

            // basic latin
            for (char32_t c = 0x0000; c <= 0x007F; ++c) {
                if (!std::isprint(static_cast<char>(c))) {
                    continue;
                }
                ss.push_back(c);
            }

            for (char32_t c = 0x4E00; c <= 0x9FFF; ++c) {
                ss.push_back(c);
            }
            for (char32_t c = 0x3000; c <= 0x303F; ++c) {
                ss.push_back(c);
            }
            for (char32_t c = 0xFF00; c <= 0xFFEF; ++c) {
                ss.push_back(c);
            }

            range.get() = utf8::utf32to8(std::u32string(ss.begin(), ss.end()));
        });

        return s_glyphRange;
    }
}// namespace game::Util