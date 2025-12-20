#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

namespace game::State {
    struct DebugToolState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "DebugToolState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

    private:
        bool m_showDebugWindow{false};
    };
}// namespace game::State