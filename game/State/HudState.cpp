#include "HudState.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"
#include "Registry.hpp"

#include "State/GamePlayData.hpp"

#include "imgui.h"

namespace game::State {
    static I18N HUD_HEALTH_TEXT("hud.health_text", U"Health: {}");
    static I18N HUD_WEAPON_TEXT("hud.weapon_text", U"Weapon: {}");

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

    void HudState::registerDebugCommands(GameManager& ctx) {
        ctx.addDebugDrawFunction(
                "HUD State",
                [this, &ctx]() {
                    ImGui::Begin("HUD State Debug Info");
                    ImGui::Text("Health: %.2f", m_health);
                    ImGui::Text("Current Weapon: %s", weaponItemToString(m_currentWeaponItem).data());

                    ImGui::Separator();
                    if (ImGui::Button("Set Health to 100")) {
                        updateHealth(100.0f);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Set Health to 50")) {
                        updateHealth(50.0f);
                    }

                    ImGui::Separator();

                    ImGui::Checkbox("Prevent Auto Image Update", &m_debugPreventAutoImageUpdate);

                    ImGui::Text("Switch Weapon:");
                    if (ImGui::Button("Primary Weapon")) {
                        updateWeapon(WeaponSlot::Primary);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Secondary Weapon")) {
                        updateWeapon(WeaponSlot::Secondary);
                    }

                    if (ImGui::Button("Set Weapon to Glock")) {
                        updateWeaponImages(State::WeaponItems::None, State::WeaponItems::Glock);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Set Weapon to USP")) {
                        updateWeaponImages(State::WeaponItems::None, State::WeaponItems::USP);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Set Weapon to Desert Eagle")) {
                        updateWeaponImages(State::WeaponItems::None, State::WeaponItems::DesertEagle);
                    }

                    if (ImGui::Button("Set Weapon to AK47")) {
                        updateWeaponImages(State::WeaponItems::AK47, State::WeaponItems::None);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Set Weapon to M4A1")) {
                        updateWeaponImages(State::WeaponItems::M4A1, State::WeaponItems::None);
                    }

                    ImGui::End();
                });
    }

    moe::ImageId HudState::getImageIdForWeaponItem(WeaponItems item) {
        switch (item) {
#define X(member, name)            \
    case State::WeaponItems::name: \
        return m_##member##ImageId;

            _GAME_HUD_STATE_WEAPON_IMAGE_LIST
#undef X
            default:
                break;
        }

        return moe::NULL_IMAGE_ID;
    }

    void HudState::updateWeaponImages(WeaponItems primary, WeaponItems secondary) {
        if (m_primaryWeapon == primary && m_secondaryWeapon == secondary) {
            return;
        }

        m_primaryWeaponImageWidget->setImageId(getImageIdForWeaponItem(primary));
        m_secondaryWeaponImageWidget->setImageId(getImageIdForWeaponItem(secondary));

        m_primaryWeapon = primary;
        m_secondaryWeapon = secondary;

        m_rootWidget->layout();

        moe::Logger::debug(
                "HudState::updateWeaponImages: primary weapon = {}, secondary weapon = {}",
                weaponItemToString(primary),
                weaponItemToString(secondary));
    }

    void HudState::loadWeaponImages(GameManager& ctx) {
#define X(name, fileName)                                                                  \
    m_##name##ImageId = m_##name##ImageLoader.generate().value_or(moe::NULL_IMAGE_ID);     \
    if (m_##name##ImageId == moe::NULL_IMAGE_ID) {                                         \
        moe::Logger::error("HudState::loadWeaponImages: failed to load image for " #name); \
    }

        _GAME_HUD_STATE_WEAPON_IMAGE_LIST

#undef X
    }

    void HudState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering HudState");

        loadWeaponImages(ctx);
        registerDebugCommands(ctx);

        m_fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (m_fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("HudState::onEnter: failed to load font");
            return;
        }

        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(width, height);

        auto virtualContainer = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
        virtualContainer->setAlign(moe::BoxAlign::Center);
        virtualContainer->setJustify(moe::BoxJustify::End);

        {
            m_bottomContainer = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Horizontal);
            m_bottomContainer->setAlign(moe::BoxAlign::End);
            m_bottomContainer->setJustify(moe::BoxJustify::SpaceBetween);
            m_bottomContainer->setPadding({10.f, 10.f, 10.f, 10.f});

            {
                m_containerHealthSet = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Horizontal);
                m_containerHealthSet->setAlign(moe::BoxAlign::Center);
                m_containerHealthSet->setJustify(moe::BoxJustify::Center);

                m_healthTextWidget = moe::makeRef<moe::VkTextWidget>(
                        Util::formatU32(U"{}", std::round(m_health)),
                        m_fontId,
                        24.f,
                        moe::Colors::White);
                m_healthTextWidget->setMargin({0.f, 0.f, 16.f, 5.f});
                m_healthProgressBarWidget = moe::makeRef<moe::VkProgressBarWidget>(
                        moe::LayoutSize{200.f, 12.f},
                        moe::Color::fromNormalized(0, 0, 0, 128),
                        moe::Colors::White);

                m_containerHealthSet->addChild(m_healthTextWidget);
                m_containerHealthSet->addChild(m_healthProgressBarWidget);
            }
            m_bottomContainer->addChild(m_containerHealthSet);

            // spacer
            m_bottomContainer->addChild(moe::makeRef<moe::SpriteWidget>(moe::LayoutSize{static_cast<float>(width - 256.0f), 1.0f}));

            {
                m_containerWeaponSet = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
                m_containerWeaponSet->setAlign(moe::BoxAlign::End);
                m_containerWeaponSet->setJustify(moe::BoxJustify::End);

                m_weaponTextWidget = moe::makeRef<moe::VkTextWidget>(
                        Util::formatU32(U"{}", "N/A"),
                        m_fontId,
                        24.f,
                        moe::Colors::White);
                m_weaponTextWidget->setMargin({0.f, 0.f, 16.f, 5.f});

                m_primaryWeaponImageWidget = moe::makeRef<moe::VkImageWidget>(
                        moe::NULL_IMAGE_ID,
                        moe::LayoutSize{200.0f, 100.0f},
                        moe::Colors::White,
                        moe::LayoutSize{400.0f, 200.0f});
                m_secondaryWeaponImageWidget = moe::makeRef<moe::VkImageWidget>(
                        moe::NULL_IMAGE_ID,
                        moe::LayoutSize{200.0f, 100.0f},
                        moe::Colors::White,
                        moe::LayoutSize{400.0f, 200.0f});

                m_containerWeaponSet->addChild(m_primaryWeaponImageWidget);
                m_containerWeaponSet->addChild(m_secondaryWeaponImageWidget);
                m_containerWeaponSet->addChild(m_weaponTextWidget);
            }
            m_bottomContainer->addChild(m_containerWeaponSet);
        }

        virtualContainer->addChild(m_bottomContainer);

        m_rootWidget->addChild(virtualContainer);

        m_rootWidget->layout();
    }

    void HudState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting HudState");

        ctx.removeDebugDrawFunction("HUD State");

        m_rootWidget.reset();
        m_bottomContainer.reset();

        m_healthTextWidget.reset();
        m_healthProgressBarWidget.reset();
        m_containerHealthSet.reset();

        m_containerWeaponSet.reset();
        m_primaryWeaponImageWidget.reset();
        m_secondaryWeaponImageWidget.reset();
        m_weaponTextWidget.reset();
    }

    void HudState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        m_healthTextWidget->render(renderer);
        m_healthProgressBarWidget->render(renderer);
        m_weaponTextWidget->render(renderer);
        m_primaryWeaponImageWidget->render(renderer);
        m_secondaryWeaponImageWidget->render(renderer);


        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData || m_debugPreventAutoImageUpdate) {
            return;
        }

        updateWeaponImages(sharedData->playerPrimaryWeapon, sharedData->playerSecondaryWeapon);
    }

    void HudState::updateHealth(float health) {
        if (std::abs(m_health - health) < 0.01f) {
            return;
        }

        m_health = health;
        m_healthTextWidget->setText(Util::formatU32(U"{}", std::round(m_health)));
        m_healthProgressBarWidget->setProgress(m_health / 100.0f);
        m_rootWidget->layout();
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
                        U"{}",
                        weaponItemToString(item)));

        moe::Logger::debug("HudState::updateWeapon: weapon updated to {}", weaponItemToString(item));

        m_rootWidget->layout();
    }
}// namespace game::State