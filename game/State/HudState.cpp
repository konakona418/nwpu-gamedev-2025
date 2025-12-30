#include "HudState.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"
#include "Registry.hpp"

#include "State/GamePlayData.hpp"

namespace game::State {
    static I18N HUD_HEALTH_TEXT("hud.health_text", U"Health: {}");
    static I18N HUD_WEAPON_TEXT("hud.weapon_text", U"Weapon: {}");

    void HudState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering HudState");

        m_fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (m_fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("HudState::onEnter: failed to load font");
            return;
        }

        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(width, height);
        auto virtualContainer = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
        virtualContainer->setAlign(moe::BoxAlign::End);//right bottom
        virtualContainer->setJustify(moe::BoxJustify::End);

        m_containerWidget = moe::makeRef<moe::VkBoxWidget>(moe::BoxLayoutDirection::Vertical);
        m_containerWidget->setPadding({10.f, 10.f, 10.f, 10.f});
        m_containerWidget->setBackgroundColor(moe::Color{0.f, 0.f, 0.f, 0.25f});

        m_healthTextWidget = moe::makeRef<moe::VkTextWidget>(
                Util::formatU32(HUD_HEALTH_TEXT.get(), std::round(m_health)),
                m_fontId,
                24.f,
                moe::Colors::White);
        m_healthTextWidget->setMargin({0.f, 0.f, 0.f, 5.f});

        m_weaponTextWidget = moe::makeRef<moe::VkTextWidget>(
                Util::formatU32(HUD_WEAPON_TEXT.get(), "N/A"),
                m_fontId,
                24.f,
                moe::Colors::White);
        m_weaponTextWidget->setMargin({0.f, 0.f, 0.f, 5.f});

        m_containerWidget->addChild(m_healthTextWidget);
        m_containerWidget->addChild(m_weaponTextWidget);

        virtualContainer->addChild(m_containerWidget);
        m_rootWidget->addChild(virtualContainer);

        m_rootWidget->layout();
    }

    void HudState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting HudState");

        m_rootWidget.reset();
        m_containerWidget.reset();
        m_healthTextWidget.reset();
        m_weaponTextWidget.reset();
    }

    void HudState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        m_containerWidget->render(renderer);
        m_healthTextWidget->render(renderer);
        m_weaponTextWidget->render(renderer);
    }

    void HudState::updateHealth(float health) {
        if (std::abs(m_health - health) < 0.01f) {
            return;
        }

        m_health = health;
        m_healthTextWidget->setText(Util::formatU32(HUD_HEALTH_TEXT.get(), std::round(m_health)));
        m_rootWidget->layout();
    }

    static moe::StringView weaponItemToString(State::WeaponItems item) {
#define X(name, enumVal, _1, _2) \
    case enumVal:                \
        return name;

        switch (item) {
            _GAME_STATE_WEAPON_ITEM_NAME_ENUM_MAP_XXX()
            case State::WeaponItems::None:
                break;
        }
#undef X

        return "N/A";
    }

    void HudState::updateWeapon(WeaponSlot weaponSlot) {
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("HudState::updateWeapon: GamePlaySharedData not found");
            return;
        }

        // default to secondary weapon
        State::WeaponItems item =
                (weaponSlot == WeaponSlot::Primary)
                        ? sharedData->playerPrimaryWeapon
                        : sharedData->playerSecondaryWeapon;

        if (m_currentWeaponItem == item) {
            return;
        }

        m_currentWeaponItem = item;
        m_weaponTextWidget->setText(
                Util::formatU32(
                        HUD_WEAPON_TEXT.get(),
                        weaponItemToString(item)));

        moe::Logger::debug("HudState::updateWeapon: weapon updated to {}", weaponItemToString(item));

        m_rootWidget->layout();
    }
}// namespace game::State