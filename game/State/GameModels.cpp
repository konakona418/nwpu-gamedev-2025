#include "State/GameModels.hpp"

namespace game::State {
    ObjectPoolRegister GameModels::PLAYER_T_MODEL_REGISTER(
            moe::asset("assets/models/T-Model.glb"),
            1, true);
    ObjectPoolRegister GameModels::PLAYER_CT_MODEL_REGISTER(
            moe::asset("assets/models/CT-Model.glb"),
            1, true);
    ObjectPoolRegister GameModels::PLAYGROUND_MODEL_REGISTER(
            moe::asset("assets/models/playground.glb"),
            1, true);
}// namespace game::State