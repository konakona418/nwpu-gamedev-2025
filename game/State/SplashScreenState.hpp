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

        float m_elapsedTimeSecs{0.0f};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_containerWidget{nullptr};
        moe::Ref<moe::VkImageWidget> m_logoImageWidget{nullptr};
        moe::ImageId m_logoImageId{moe::NULL_IMAGE_ID};

        Tween<QuadraticProvider, QuadraticProvider> m_fadeTween{0.2f, 0.8f};
    };
}// namespace game::State