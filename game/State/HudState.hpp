#pragma once

#include "GameState.hpp"

#include "State/GameCommon.hpp"

#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkBoxWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "Util.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"


namespace game::State {
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

        float m_health{0.0f};
        WeaponItems m_currentWeaponItem{WeaponItems::None};

        FontLoaderT m_fontLoader{
                FontLoaderParam{24.0f, Util::glyphRangeChinese()},
                moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf"))};
        moe::FontId m_fontId{moe::NULL_FONT_ID};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_containerWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_healthTextWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_weaponTextWidget{nullptr};
    };
}// namespace game::State