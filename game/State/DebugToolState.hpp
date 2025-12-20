#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"
#include "Input.hpp"

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
        InputProxy m_inputProxy{InputProxy::PRIORITY_SYSTEM};
        bool m_showDebugWindow{false};
    };
}// namespace game::State