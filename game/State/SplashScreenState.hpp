#pragma once

#include "GameState.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "Tween.hpp"
#include "Util.hpp"


#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkBoxWidget.hpp"
#include "UI/Vulkan/VkImageWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"


#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"


namespace game::State {
    struct SplashScreenState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "SplashScreenState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

    private:
        using FontLoaderT =
                moe::Secure<game::AnyCacheLoader<game::FontLoader<
                        moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;
        FontLoaderT m_fontLoader{
                FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf"))};

        float m_elapsedTimeSecs{0.0f};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_containerWidget{nullptr};
        moe::Ref<moe::VkImageWidget> m_logoImageWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_loadingTextWidget{nullptr};
        moe::ImageId m_logoImageId{moe::NULL_IMAGE_ID};

        Tween<QuadraticProvider, QuadraticProvider> m_fadeInTween{1.0f, 0.0f};
        Tween<QuadraticProvider, QuadraticProvider> m_fadeOutTween{0.0f, 1.0f};

        enum class SplashPhase {
            FadeIn,
            Display,
            FadeOut,
        };
        SplashPhase m_currentPhase{SplashPhase::FadeIn};

        size_t m_totalObjectsToLoad{0};
        size_t m_loadedObjectsCount{0};
    };
}// namespace game::State