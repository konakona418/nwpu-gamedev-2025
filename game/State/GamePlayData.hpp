#pragma once

#include "Core/Common.hpp"

namespace game {
    constexpr uint16_t INVALID_PLAYER_TEMP_ID = 0xFFFF;

    struct GamePlaySharedData {
        uint16_t playerTempId{0};
        uint32_t playerBalance{0};
    };
}// namespace game