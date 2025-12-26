#pragma once

#include "Core/Common.hpp"
#include <utf8.h>

namespace game::Util {
    moe::StringView glyphRangeChinese();

    struct TimePack {
        size_t physicsTick;
        uint64_t currentTimeMillis;
    };

    TimePack getTimePack();

    template<typename... Args>
    moe::U32String formatU32(moe::U32StringView fmt, Args&&... args) {
        auto result = fmt::format(fmt::runtime(utf8::utf32to8(fmt)), std::forward<Args>(args)...);
        return utf8::utf8to32(result);
    }
}// namespace game::Util