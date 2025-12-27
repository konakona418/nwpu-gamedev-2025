#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

namespace game::State {
    struct RemotePlayerState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "RemotePlayerState";
        }

        void onEnter(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        void onExit(GameManager& ctx) override;
    };
}// namespace game::State