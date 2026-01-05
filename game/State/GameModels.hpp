#pragma once

#include "ObjectPool.hpp"

namespace game::State {
    struct GameModels {
        static ObjectPoolRegister PLAYER_T_MODEL_REGISTER;
        static ObjectPoolRegister PLAYER_CT_MODEL_REGISTER;
        static ObjectPoolRegister PLAYGROUND_MODEL_REGISTER;
    };
}// namespace game::State