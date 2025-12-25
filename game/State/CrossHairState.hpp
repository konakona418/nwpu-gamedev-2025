#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"


namespace game::State {
    struct CrossHairState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "CrossHairState";
        }

        void onUpdate(GameManager& ctx, float deltaTime) override;
    };
}// namespace game::State