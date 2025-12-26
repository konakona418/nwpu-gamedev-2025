#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

#include "AnyCache.hpp"
#include "GPUImageLoader.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/ImageLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

#include "UI/BoxWidget.hpp"
#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkImageWidget.hpp"


namespace game::State {
    struct CrossHairState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "CrossHairState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

    private:
        using LoaderT = moe::Secure<game::AnyCacheLoader<game::GPUImageLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<
                        moe::ImageLoader<moe::BinaryLoader>>>>>>>;

        LoaderT m_crossHairImageLoader{
                moe::BinaryFilePath(moe::asset("assets/images/crosshair-normal.png")),
        };
        moe::ImageId m_crossHairShotImageId{moe::NULL_IMAGE_ID};

        LoaderT m_crossHairShotImageLoader{
                moe::BinaryFilePath(moe::asset("assets/images/crosshair-shot.png")),
        };
        moe::ImageId m_crossHairNormalImageId{moe::NULL_IMAGE_ID};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkImageWidget> m_crossHairImageWidget{nullptr};

        // todo: add crosshair state (normal, shot, etc.) and logic to switch between them
    };
}// namespace game::State