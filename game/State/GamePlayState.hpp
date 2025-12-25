#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"


namespace game::State {
    struct GamePlayState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "GamePlayState";
        }

        void onEnter(GameManager& ctx) override;
        // void onExit(GameManager& ctx) override;
    };
}// namespace game::State