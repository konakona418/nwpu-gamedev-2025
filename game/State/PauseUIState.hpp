#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

#include "UI/BoxWidget.hpp"
#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkButtonWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"

namespace game::State {
    struct PauseUIState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "PauseUIState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

    private:
        InputLockToken m_inputLockToken{NO_LOCK_TOKEN};

        // ! fixme: font loading will be triggered every onEnter; optimize later
        moe::FontId m_fontId{moe::NULL_FONT_ID};

        moe::Ref<moe::RootWidget> m_rootWidget;
        moe::Ref<moe::VkTextWidget> m_titleTextWidget;
        moe::Ref<moe::VkButtonWidget> m_resumeButtonWidget;
    };
}// namespace game::State