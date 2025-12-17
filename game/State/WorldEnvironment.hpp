#pragma once

#include "GameState.hpp"

namespace game::State {
    struct WorldEnvironment : public GameState {
        const moe::StringView getName() const override {
            return "WorldEnvironment";
        }

        void onUpdate(GameManager& ctx, float deltaTime) override;
    };
}// namespace game::State