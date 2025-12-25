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
}// namespace game::Util