#pragma once

#include "Core/Common.hpp"

namespace game {
    struct GamePlaySharedData {
        uint16_t playerTempId{0};
        uint32_t playerBalance{0};
    };
}// namespace game