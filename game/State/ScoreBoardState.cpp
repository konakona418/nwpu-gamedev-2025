#include "State/ScoreBoardState.hpp"

#include "State/HudSharedConfig.hpp"

#include "GameManager.hpp"
#include "Registry.hpp"
#include "Util.hpp"

#include "imgui.h"

namespace game::State {
    static ParamF BOMB_ICON_TRANSITION_ANIMATION_DURATION("scoreboard.bomb_icon_transition_duration", 1.6f);
    static ParamF BOMB_ICON_SIZE("scoreboard.bomb_icon_size", 50.0f);
    static ParamF BOMB_ICON_MIN_ALPHA("scoreboard.bomb_icon_min_alpha", 0.5f);

    void ScoreBoardState::updateScore(GamePlayerTeam winningTeam) {
        if (winningTeam == GamePlayerTeam::T) {
            m_terroristsScore++;
        } else if (winningTeam == GamePlayerTeam::CT) {
            m_counterTerroristsScore++;
        }

        m_counterTerroristsScoreWidget->setText(Util::formatU32(U"{}", m_counterTerroristsScore));
        m_terroristsScoreWidget->setText(Util::formatU32(U"{}", m_terroristsScore));

        moe::Logger::debug(
                "ScoreBoardState::updateScore: updated score - CT: {}, T: {}",
                m_counterTerroristsScore,
                m_terroristsScore);

        m_rootWidget->layout();
    }

    void ScoreBoardState::updateRemainingPlayers(uint16_t whoDied) {
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("ScoreBoardState::updateRemainingPlayers: GamePlaySharedData not found");
            return;
        }

        auto player = sharedData->playersByTempId.find(whoDied);
        if (player == sharedData->playersByTempId.end()) {
            moe::Logger::error("ScoreBoardState::updateRemainingPlayers: Player with temp ID {} not found", whoDied);
            return;
        }

        auto team = player->second.team;
        if (team == GamePlayerTeam::T) {
            m_remainingTerrorists = std::max(0, static_cast<int>(m_remainingTerrorists) - 1);
        } else if (team == GamePlayerTeam::CT) {
            m_remainingCounterTerrorists = std::max(0, static_cast<int>(m_remainingCounterTerrorists) - 1);
        }

        m_remainingPlayersTextWidget->setText(
                Util::formatU32(U"{} vs {}", m_remainingCounterTerrorists, m_remainingTerrorists));

        moe::Logger::debug("ScoreBoardState::updateRemainingPlayers: updated remaining players - CT: {}, T: {}",
                           m_remainingCounterTerrorists,
                           m_remainingTerrorists);

        m_rootWidget->layout();
    }

    void ScoreBoardState::updateBombStatus(bool isPlanted) {
        if (isPlanted == m_isBombPlanted) {
            return;
        }

        m_isBombPlanted = isPlanted;

        moe::Logger::debug("ScoreBoardState::updateBombStatus: bomb status updated - isPlanted: {}", m_isBombPlanted);

        auto iconSize = BOMB_ICON_SIZE.get();
        if (m_isBombPlanted) {
            m_bombStatusIconWidget->setSpriteSize(moe::LayoutSize{iconSize, iconSize});
        } else {
            m_bombStatusIconWidget->setSpriteSize(moe::LayoutSize{0.0f, 0.0f});
        }

        m_rootWidget->layout();
    }

    void ScoreBoardState::resetRound() {
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("ScoreBoardState::resetRound: GamePlaySharedData not found");
            return;
        }

        auto playerCount = sharedData->playersByTempId.size();
        m_remainingTerrorists = playerCount / 2;
        m_remainingCounterTerrorists = playerCount - m_remainingTerrorists;

        // reset countdown timer
        m_countDownTimeSeconds = 0.0f;
        m_countDownTextWidget->setText(U"00:00");

        // invalidate bomb status icon
        m_isBombPlanted = false;
        m_bombStatusIconWidget->setSpriteSize(moe::LayoutSize{0.0f, 0.0f});

        m_remainingPlayersTextWidget->setText(
                Util::formatU32(U"{} vs {}", m_remainingCounterTerrorists, m_remainingTerrorists));

        m_rootWidget->layout();
    }

    void ScoreBoardState::setCountDownTime(uint32_t msLeft) {
        m_countDownTimeSeconds = static_cast<float>(msLeft) / 1000.0f;
        m_countDownDeltaAccum = 0.0f;

        uint32_t totalSeconds = static_cast<uint32_t>(m_countDownTimeSeconds);
        uint32_t minutes = totalSeconds / 60;
        uint32_t seconds = totalSeconds % 60;

        m_countDownTextWidget->setText(Util::formatU32(U"{:02}:{:02}", minutes, seconds));

        moe::Logger::debug("ScoreBoardState::setCountDownTime: countdown time set to {} ms ({}:{:02})",
                           msLeft,
                           minutes,
                           seconds);

        m_rootWidget->layout();
    }

    void ScoreBoardState::registerDebugCommands(GameManager& ctx) {
        ctx.addDebugDrawFunction(
                "ScoreBoard State Debug",
                [this]() {
                    ImGui::Begin("ScoreBoard State Debug");

                    ImGui::Text("Counter-Terrorists Score: %d", m_counterTerroristsScore);
                    ImGui::Text("Terrorists Score: %d", m_terroristsScore);
                    ImGui::Text("Remaining Counter-Terrorists: %d", m_remainingCounterTerrorists);
                    ImGui::Text("Remaining Terrorists: %d", m_remainingTerrorists);
                    ImGui::Text("Bomb Planted: %s", m_isBombPlanted ? "Yes" : "No");

                    ImGui::Separator();

                    if (ImGui::Button("Plant bomb")) {
                        updateBombStatus(true);
                    }

                    if (ImGui::Button("Defuse bomb")) {
                        updateBombStatus(false);
                    }

                    if (ImGui::Button("Reset round")) {
                        resetRound();
                    }

                    static int countdownSecs = 0;
                    ImGui::SliderInt("Countdown", &countdownSecs, 1, 600);
                    if (ImGui::Button("Set Countdown")) {
                        setCountDownTime(countdownSecs * 1000);
                    }

                    ImGui::End();
                });
    }

    void ScoreBoardState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering ScoreBoardState");

        registerDebugCommands(ctx);

        m_fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (m_fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("ScoreBoardState::onEnter: failed to load font");
            return;
        }

        m_bombIconId = m_bombIconLoader.generate().value_or(moe::NULL_IMAGE_ID);
        if (m_bombIconId == moe::NULL_IMAGE_ID) {
            moe::Logger::error("ScoreBoardState::onEnter: failed to load bomb icon image");
            return;
        }

        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(width, height);

        auto virtualContainer = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
        virtualContainer->setAlign(moe::BoxAlign::Center);
        virtualContainer->setJustify(moe::BoxJustify::Start);// topmost, centered

        {
            m_topContainer = moe::makeRef<moe::VkBoxWidget>(moe::BoxLayoutDirection::Vertical);
            m_topContainer->setAlign(moe::BoxAlign::Center);
            m_topContainer->setJustify(moe::BoxJustify::Start);
            m_topContainer->setBackgroundColor(moe::Color::fromNormalized(0, 0, 0, 64));
            m_topContainer->setPadding({10.0f, 10.0f, 10.0f, 10.0f});

            {
                m_countDownTextWidget = moe::makeRef<moe::VkTextWidget>(
                        U"00:00",
                        m_fontId,
                        24.f,
                        moe::Colors::White);
                m_countDownTextWidget->setMargin({0.f, 0.f, 0.f, 5.f});

                m_topContainer->addChild(m_countDownTextWidget);
            }

            {
                // by default, hide the icon by setting its size to zero
                m_bombStatusIconWidget = moe::makeRef<moe::VkImageWidget>(
                        m_bombIconId,
                        moe::LayoutSize{0.0f, 0.0f},
                        moe::Colors::White,
                        moe::LayoutSize{50.0f, 50.0f});

                m_topContainer->addChild(m_bombStatusIconWidget);
            }

            {
                m_scoreBoardSet = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Horizontal);
                m_scoreBoardSet->setAlign(moe::BoxAlign::Center);
                m_scoreBoardSet->setJustify(moe::BoxJustify::Center);

                {
                    auto ctColor = HudConfig::HUD_COUNTERTERRORIST_TINT_COLOR.get();
                    auto tColor = HudConfig::HUD_TERRORIST_TINT_COLOR.get();

                    m_counterTerroristsScoreWidget = moe::makeRef<moe::VkTextWidget>(
                            Util::formatU32(U"{}", m_counterTerroristsScore),
                            m_fontId,
                            32.f,
                            moe::Color{ctColor.x, ctColor.y, ctColor.z, ctColor.w});
                    m_counterTerroristsScoreWidget->setMargin({0.f, 0.f, 16.f, 5.f});

                    m_terroristsScoreWidget = moe::makeRef<moe::VkTextWidget>(
                            Util::formatU32(U"{}", m_terroristsScore),
                            m_fontId,
                            32.f,
                            moe::Color{tColor.x, tColor.y, tColor.z, tColor.w});
                    m_terroristsScoreWidget->setMargin({16.f, 0.f, 0.f, 5.f});
                }
                m_scoreBoardSet->addChild(m_counterTerroristsScoreWidget);
                m_scoreBoardSet->addChild(m_terroristsScoreWidget);
            }
            m_topContainer->addChild(m_scoreBoardSet);

            {
                m_remainingPlayersSet = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Horizontal);
                m_remainingPlayersSet->setAlign(moe::BoxAlign::Center);
                m_remainingPlayersSet->setJustify(moe::BoxJustify::Center);

                {
                    m_remainingPlayersTextWidget = moe::makeRef<moe::VkTextWidget>(
                            Util::formatU32(U"{} vs {}", m_remainingCounterTerrorists, m_remainingTerrorists),
                            m_fontId,
                            18.f,
                            moe::Colors::White);
                }
                m_remainingPlayersSet->addChild(m_remainingPlayersTextWidget);
            }
            m_topContainer->addChild(m_remainingPlayersSet);
        }

        virtualContainer->addChild(m_topContainer);
        m_rootWidget->addChild(virtualContainer);

        m_rootWidget->layout();
    }

    void ScoreBoardState::updateBombIconTransparency(float deltaTime) {
        // time for animation update
        m_timeAccum += deltaTime;
        if (m_timeAccum > BOMB_ICON_TRANSITION_ANIMATION_DURATION.get()) {
            m_timeAccum = 0.0f;
        }

        // animation bomb icon transparency
        float ratio = moe::Math::clamp(m_timeAccum / BOMB_ICON_TRANSITION_ANIMATION_DURATION.get(), 0.0f, 1.0f);
        auto tweenValue = m_bombIconTransparencyTween.eval(ratio);
        auto minAlpha = BOMB_ICON_MIN_ALPHA.get();
        float alpha = minAlpha + (1.0f - minAlpha) * tweenValue;
        m_bombStatusIconWidget->setTintColor(
                moe::Color{
                        1.0f,
                        1.0f,
                        1.0f,
                        alpha,
                });
    }

    void ScoreBoardState::updateCountDownTextWidget(float deltaTime) {
        m_countDownTimeSeconds -= deltaTime;
        if (m_countDownTimeSeconds < 0.0f) {
            // clamp to zero
            m_countDownTimeSeconds = 0.0f;
        }


        m_countDownDeltaAccum += deltaTime;
        if (m_countDownDeltaAccum < 1.0f) {
            // only update every second
            return;
        }

        m_countDownDeltaAccum = 0.0f;

        uint32_t totalSeconds = static_cast<uint32_t>(std::round(m_countDownTimeSeconds));
        uint32_t minutes = totalSeconds / 60;
        uint32_t seconds = totalSeconds % 60;
        m_countDownTextWidget->setText(Util::formatU32(U"{:02}:{:02}", minutes, seconds));

        // layout update
        m_rootWidget->layout();
    }

    void ScoreBoardState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        // update countdown timer
        updateCountDownTextWidget(deltaTime);
        // animate bomb icon transparency
        updateBombIconTransparency(deltaTime);

        m_topContainer->render(renderer);
        m_countDownTextWidget->render(renderer);
        m_bombStatusIconWidget->render(renderer);
        m_counterTerroristsScoreWidget->render(renderer);
        m_terroristsScoreWidget->render(renderer);
        m_remainingPlayersTextWidget->render(renderer);
    }

    void ScoreBoardState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting ScoreBoardState");

        ctx.removeDebugDrawFunction("ScoreBoard State Debug");

        m_rootWidget.reset();
        m_topContainer.reset();
        m_scoreBoardSet.reset();
        m_counterTerroristsScoreWidget.reset();
        m_terroristsScoreWidget.reset();
        m_remainingPlayersSet.reset();
        m_remainingPlayersTextWidget.reset();
    }
}// namespace game::State