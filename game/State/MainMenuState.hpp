#pragma once

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "GameState.hpp"
#include "Input.hpp"
#include "Util.hpp"

#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkButtonWidget.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

namespace game::State {
    struct MainMenuState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "MainMenuState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

    private:
        moe::Preload<game::AnyCacheLoader<moe::Secure<game::FontLoader<
                game::AnyCacheLoader<moe::Launch<moe::BinaryLoader>>>>>>
                m_fontId{
                        FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                        moe::BinaryFilePath(
                                moe::asset("assets/fonts/NotoSansSC-Regular.ttf"))};

        InputProxy m_inputProxy{InputProxy::PRIORITY_UI_LOCK};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_multiPlayerButtonWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_settingsButtonWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_creditsButtonWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_exitButtonWidget{nullptr};
    };
}// namespace game::State