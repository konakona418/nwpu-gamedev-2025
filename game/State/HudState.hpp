#pragma once

#include "GameState.hpp"

#include "State/GameCommon.hpp"
#include "State/GamePlayData.hpp"

#include "UI/BoxWidget.hpp"
#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkImageWidget.hpp"
#include "UI/Vulkan/VkProgressBarWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"


#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "GPUImageLoader.hpp"
#include "Util.hpp"


#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/ImageLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"


namespace game::State {

#define _GAME_HUD_STATE_WEAPON_IMAGE_LIST \
    X(usp, USP)                           \
    X(glock, Glock)                       \
    X(desertEagle, DesertEagle)           \
    X(ak47, AK47)                         \
    X(m4a1, M4A1)

    struct HudState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "HUDState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

        void updateHealth(float health);
        void updateWeapon(WeaponSlot weaponSlot);

    private:
        using FontLoaderT = moe::Secure<game::AnyCacheLoader<game::FontLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;

        using ImageLoaderT = moe::Secure<game::AnyCacheLoader<game::GPUImageLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<
                        moe::ImageLoader<moe::BinaryLoader>>>>>>>;

        float m_health{0.0f};
        WeaponItems m_currentWeaponItem{WeaponItems::None};

        FontLoaderT m_fontLoader{
                FontLoaderParam{24.0f},
                moe::BinaryFilePath(moe::asset("assets/fonts/orbitron/Orbitron-Medium.ttf"))};
        moe::FontId m_fontId{moe::NULL_FONT_ID};

#define X(name, fileName)                                                               \
    ImageLoaderT m_##name##ImageLoader{                                                 \
            moe::BinaryFilePath(moe::asset("assets/images/weapons/" #fileName ".png")), \
    };                                                                                  \
    moe::ImageId m_##name##ImageId{moe::NULL_IMAGE_ID};

        _GAME_HUD_STATE_WEAPON_IMAGE_LIST

#undef X

        void loadWeaponImages(GameManager& ctx);

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};

        moe::Ref<moe::BoxWidget> m_bottomContainer{nullptr};

        moe::Ref<moe::BoxWidget> m_containerHealthSet{nullptr};
        moe::Ref<moe::VkTextWidget> m_healthTextWidget{nullptr};
        moe::Ref<moe::VkProgressBarWidget> m_healthProgressBarWidget{nullptr};

        moe::Ref<moe::BoxWidget> m_containerWeaponSet{nullptr};
        moe::Ref<moe::VkImageWidget> m_primaryWeaponImageWidget{nullptr};
        moe::Ref<moe::VkImageWidget> m_secondaryWeaponImageWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_weaponTextWidget{nullptr};

        void registerDebugCommands(GameManager& ctx);

        WeaponSlot m_currentSlot{WeaponSlot::Primary};

        WeaponItems m_primaryWeapon{WeaponItems::None};
        WeaponItems m_secondaryWeapon{WeaponItems::None};
        bool m_debugPreventAutoImageUpdate{false};
        void updateWeaponImages(WeaponItems primary, WeaponItems secondary);

        GamePlayerTeam m_playerTeam{GamePlayerTeam::None};
        void updatePlayerTeam(GamePlayerTeam team);

        moe::Color m_currentTintColor{moe::Colors::White};
        void applyTintColorToUIWidgets();

        moe::ImageId getImageIdForWeaponItem(WeaponItems item);
    };
}// namespace game::State