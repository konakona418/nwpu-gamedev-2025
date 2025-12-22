#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"
#include "Input.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "Util.hpp"

#include "UI/BoxWidget.hpp"
#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkButtonWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"


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
        InputProxy m_inputProxy{InputProxy::PRIORITY_UI_LOCK};

        moe::Preload<game::AnyCacheLoader<moe::Secure<game::FontLoader<
                game::AnyCacheLoader<moe::Launch<moe::BinaryLoader>>>>>>
                m_fontId{
                        FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                        moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf"))};

        moe::Ref<moe::RootWidget> m_rootWidget;
        moe::Ref<moe::VkTextWidget> m_titleTextWidget;
        moe::Ref<moe::VkButtonWidget> m_resumeButtonWidget;
    };
}// namespace game::State