#pragma once

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "GameState.hpp"
#include "Input.hpp"
#include "ModelLoader.hpp"
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

        void onStateChanged(GameManager& ctx, bool isTopmostState) override;

    private:
        using FontLoaderT =
                moe::Secure<game::AnyCacheLoader<game::FontLoader<
                        moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;
        using ModelLoaderT = moe::Preload<moe::Secure<game::AnyCacheLoader<game::ModelLoader>>>;

        FontLoaderT m_fontId{
                FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf")),
        };

        ModelLoaderT m_playgroundModelLoader{
                ModelLoaderParam{moe::asset("assets/models/playground.glb")},
        };
        moe::RenderableId m_playgroundRenderable{moe::NULL_RENDERABLE_ID};

        ModelLoaderT m_counterTerroristModelLoader{
                ModelLoaderParam{moe::asset("assets/models/CT-Model.glb")},
        };
        moe::RenderableId m_counterTerroristRenderable{moe::NULL_RENDERABLE_ID};
        moe::AnimationId m_counterTerroristIdleAnimation{moe::NULL_ANIMATION_ID};
        static constexpr float c_counterTerroristIdleAnimationDurationSecs{10};
        float m_counterTerroristIdleAnimationProgressSecs{0};

        InputProxy m_inputProxy{InputProxy::PRIORITY_UI_LOCK};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_titleTextWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_multiPlayerButtonWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_settingsButtonWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_creditsButtonWidget{nullptr};
        moe::Ref<moe::VkButtonWidget> m_exitButtonWidget{nullptr};

        void initModels(GameManager& ctx);
    };
}// namespace game::State