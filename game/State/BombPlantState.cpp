#include "State/BombPlantState.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"
#include "Registry.hpp"

#include "State/GamePlayData.hpp"

#include "UI/BoxWidget.hpp"

#include "imgui.h"

namespace game::State {
    static I18N BOMB_PLANTING_TEXT_PROMPT("bomb_planting.text", U"Press [E] to Plant the Bomb");
    static I18N BOMB_DEFUSING_TEXT_PROMPT("bomb_defusing.text", U"Press [E] to Defuse the Bomb");
    static I18N BOMB_PLANTING_TEXT("bomb_planting.planting", U"Planting... {}/100%");
    static I18N BOMB_DEFUSING_TEXT("bomb_defusing.defusing", U"Defusing... {}/100%");

    void BombPlantState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering BombPlantState");

        auto fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("BombPlantState::onEnter: failed to load font");
            return;
        }
        m_fontId = fontId;

        ctx.addDebugDrawFunction(
                "BombPlantState Debug Draw",
                [state = this->asRef<BombPlantState>(), &ctx]() mutable {
                    ImGui::Begin("BombPlantState Debug Draw");
                    ImGui::Text("isVisible: %s", state->isVisible() ? "true" : "false");
                    ImGui::Text("plantingProgress: %.2f", state->getPlantingProgress());

                    ImGui::Separator();
                    if (ImGui::Button("Force visible")) {
                        state->setVisible(true);
                    }
                    if (ImGui::Button("Force invisible")) {
                        state->setVisible(false);
                    }
                    ImGui::End();
                });

        // setup UI
        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(width, height);
        auto virtualContainer = moe::Ref(new moe::BoxWidget(moe::BoxLayoutDirection::Vertical));
        virtualContainer->setAlign(moe::BoxAlign::Center);
        virtualContainer->setJustify(moe::BoxJustify::End);
        virtualContainer->setPadding({0.f, 0.f, 0.f, 100.f});

        m_containerWidget = moe::Ref(new moe::VkBoxWidget(moe::BoxLayoutDirection::Vertical));
        m_containerWidget->setBackgroundColor(moe::Color{0.f, 0.f, 0.f, 0.75f});
        m_containerWidget->setPadding({10.f, 10.f, 10.f, 10.f});

        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("BombPlantState::onEnter: GamePlaySharedData not found");
        }
        auto team = sharedData ? sharedData->playerTeam : GamePlayerTeam::None;

        m_plantingTextWidget = moe::makeRef<moe::VkTextWidget>(
                team == GamePlayerTeam::T
                        ? BOMB_PLANTING_TEXT_PROMPT.get()
                        : BOMB_DEFUSING_TEXT_PROMPT.get(),
                m_fontId,
                24.f,
                moe::Colors::Yellow);

        m_containerWidget->addChild(m_plantingTextWidget);
        virtualContainer->addChild(m_containerWidget);

        m_rootWidget->addChild(virtualContainer);

        m_rootWidget->layout();
    }

    void BombPlantState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting BombPlantState");

        ctx.removeDebugDrawFunction("BombPlantState Debug Draw");

        m_rootWidget.reset();
        m_containerWidget.reset();
        m_plantingTextWidget.reset();
    }

    void BombPlantState::onUpdate(GameManager& ctx, float deltaTime) {
        if (!m_isVisible) {
            return;
        }

        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_containerWidget->render(renderer);
        m_plantingTextWidget->render(renderer);
    }

    void BombPlantState::setPlantingProgress(float progress) {
        if (std::abs(m_plantingProgress - progress) < 0.001f) {
            return;
        }

        if (progress < 0.0f || progress > 1.0f) {
            moe::Logger::warn(
                    "BombPlantState::setPlantingProgress: progress {} out of range [0.0, 1.0]",
                    progress);
        }

        m_plantingProgress = moe::Math::clamp(progress, 0.0f, 1.0f);
        uint32_t percent = std::round(m_plantingProgress * 100.0f);

        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("BombPlantState::setPlantingProgress: GamePlaySharedData not found");
            return;
        }

        auto team = sharedData->playerTeam;

        if (percent <= 1) {
            m_plantingTextWidget->setText(
                    team == GamePlayerTeam::T
                            ? BOMB_PLANTING_TEXT_PROMPT.get()
                            : BOMB_DEFUSING_TEXT_PROMPT.get());
        } else {
            m_plantingTextWidget->setText(
                    Util::formatU32(
                            team == GamePlayerTeam::T
                                    ? BOMB_PLANTING_TEXT.get()
                                    : BOMB_DEFUSING_TEXT.get(),
                            percent));
        }

        m_rootWidget->layout();
    }
}// namespace game::State