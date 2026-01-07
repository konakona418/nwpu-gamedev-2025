#include "State/ScoreDetailState.hpp"

#include "State/GamePlayData.hpp"
#include "State/HudSharedConfig.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"
#include "Registry.hpp"

#include "UI/SpriteWidget.hpp"

#include "imgui.h"

namespace game::State {
    static I18N SCOREBOARD_TITLE("scoreboard.title", U"Scoreboard");
    static I18N SCOREBOARD_PLAYER_NAME("scoreboard.player_name", U"Player Name");
    static I18N SCOREBOARD_KILLS("scoreboard.kills", U"Kills");
    static I18N SCOREBOARD_DEATHS("scoreboard.deaths", U"Deaths");

    void ScoreDetailState::initWidgets(GameManager& ctx) {
        auto [width, height] = ctx.renderer().getCanvasSize();

        m_rootWidget = moe::Ref(new moe::RootWidget(width, height));
        {
            m_baseContainer = moe::Ref(new moe::VkBoxWidget(moe::BoxLayoutDirection::Vertical));
            m_baseContainer->setAlign(moe::BoxAlign::Center);
            m_baseContainer->setJustify(moe::BoxJustify::Center);//center aligned
            m_baseContainer->setPadding({16.0f, 16.0f, 16.0f, 16.0f});
            m_baseContainer->setBackgroundColor(moe::Color::fromNormalized(0, 0, 0, 196));

            {
                m_titleTextWidget = moe::Ref(new moe::VkTextWidget(SCOREBOARD_TITLE.get(), m_fontId, 24.0f, moe::Colors::White));

                m_scoreListContainer = moe::Ref(new moe::BoxWidget(moe::BoxLayoutDirection::Horizontal));
                m_scoreListContainer->setAlign(moe::BoxAlign::Start);
                m_scoreListContainer->setJustify(moe::BoxJustify::Center);

                // spacer for columns
                // 1/3 width each for name, kills, deaths
                auto phantomWidget =
                        moe::Ref(new moe::SpriteWidget(
                                moe::LayoutSize{(static_cast<float>(width) - 128.0f) / 3, 1.0f}));
                {
                    m_playerNameColumn = moe::Ref(new moe::BoxWidget(moe::BoxLayoutDirection::Vertical));
                    m_playerNameColumn->setAlign(moe::BoxAlign::Center);
                    m_playerNameColumn->setJustify(moe::BoxJustify::Start);

                    m_playerNameColumn->addChild(phantomWidget);

                    // header
                    auto nameHeader = moe::Ref(new moe::VkTextWidget(SCOREBOARD_PLAYER_NAME.get(), m_fontId, 20.0f, moe::Colors::White));
                    m_playerNameColumn->addChild(nameHeader);
                    m_entryWidgets.push_back(nameHeader);

                    m_scoreListContainer->addChild(m_playerNameColumn);
                }

                {
                    m_killsColumn = moe::Ref(new moe::BoxWidget(moe::BoxLayoutDirection::Vertical));
                    m_killsColumn->setAlign(moe::BoxAlign::Center);
                    m_killsColumn->setJustify(moe::BoxJustify::Start);

                    m_killsColumn->addChild(phantomWidget);

                    // header
                    auto killsHeader = moe::Ref(new moe::VkTextWidget(SCOREBOARD_KILLS.get(), m_fontId, 20.0f, moe::Colors::White));
                    m_killsColumn->addChild(killsHeader);
                    m_entryWidgets.push_back(killsHeader);

                    m_scoreListContainer->addChild(m_killsColumn);
                }

                {
                    m_deathsColumn = moe::Ref(new moe::BoxWidget(moe::BoxLayoutDirection::Vertical));
                    m_deathsColumn->setAlign(moe::BoxAlign::Center);
                    m_deathsColumn->setJustify(moe::BoxJustify::Start);

                    m_deathsColumn->addChild(phantomWidget);

                    // header
                    auto deathsHeader = moe::Ref(new moe::VkTextWidget(SCOREBOARD_DEATHS.get(), m_fontId, 20.0f, moe::Colors::White));
                    m_deathsColumn->addChild(deathsHeader);
                    m_entryWidgets.push_back(deathsHeader);

                    m_scoreListContainer->addChild(m_deathsColumn);
                }

                m_baseContainer->addChild(m_titleTextWidget);
                m_baseContainer->addChild(m_scoreListContainer);
            }

            m_rootWidget->addChild(m_baseContainer);
        }
    }

    moe::Vector<ScoreDetailState::TempPlayerScoreEntry> ScoreDetailState::generateScoreEntriesFromSharedData(moe::Ref<GamePlaySharedData> sharedData) {
        auto ctColor_ = HudConfig::HUD_COUNTERTERRORIST_TINT_COLOR.get();
        auto tColor_ = HudConfig::HUD_TERRORIST_TINT_COLOR.get();

        auto ctColor = moe::Color{ctColor_.x, ctColor_.y, ctColor_.z, 1.0f};
        auto tColor = moe::Color{tColor_.x, tColor_.y, tColor_.z, 1.0f};

        moe::Vector<TempPlayerScoreEntry> entries;
        for (const auto& [_, player]: sharedData->playersByTempId) {
            entries.push_back({
                    .playerName = player.name,
                    .nameColor = player.team == GamePlayerTeam::CT ? ctColor : tColor,
                    .kills = player.kills,
                    .deaths = player.deaths,
            });
        }

        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b) {
                      return a.kills > b.kills;
                  });

        return entries;
    }

    moe::Vector<ScoreDetailState::TempPlayerScoreEntry> ScoreDetailState::generateScoreEntriesMock() {
        moe::Vector<TempPlayerScoreEntry> entries;

        auto ctColor_ = HudConfig::HUD_COUNTERTERRORIST_TINT_COLOR.get();
        auto tColor_ = HudConfig::HUD_TERRORIST_TINT_COLOR.get();

        auto ctColor = moe::Color{ctColor_.x, ctColor_.y, ctColor_.z, 1.0f};
        auto tColor = moe::Color{tColor_.x, tColor_.y, tColor_.z, 1.0f};

        struct MockInfo {
            moe::U32String name;
            bool isCT;
        };

        moe::Vector<MockInfo> rawData = {
                {U"s1mple", true},
                {U"ZywOo", true},
                {U"NiKo", true},
                {U"m0NESY", true},
                {U"ropz", true},
                {U"ELiGE", false},
                {U"Twistzz", false},
                {U"dev1ce", false},
                {U"shox", false},
                {U"Fallen", false},
        };

        for (const auto& data: rawData) {
            TempPlayerScoreEntry entry;
            entry.playerName = data.name;
            entry.nameColor = data.isCT ? ctColor : tColor;

            entry.kills = static_cast<uint16_t>(std::rand() % 31);
            entry.deaths = static_cast<uint16_t>(std::rand() % 21);

            entries.push_back(std::move(entry));
        }

        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b) {
                      return a.kills > b.kills;
                  });

        return entries;
    }

    void ScoreDetailState::populateScoreEntriesFrom(GameManager& ctx, const moe::Vector<TempPlayerScoreEntry>& entries) {
        for (auto entry: entries) {
            // player name
            auto nameWidget =
                    moe::Ref(new moe::VkTextWidget(
                            entry.playerName,
                            m_fontId,
                            18.0f,
                            entry.nameColor));
            m_playerNameColumn->addChild(nameWidget);
            m_entryWidgets.push_back(nameWidget);

            // kills
            auto killsWidget = moe::Ref(new moe::VkTextWidget(
                    Util::formatU32(U"{}", entry.kills),
                    m_fontId,
                    18.0f,
                    moe::Colors::White));
            m_killsColumn->addChild(killsWidget);
            m_entryWidgets.push_back(killsWidget);

            // deaths
            auto deathsWidget = moe::Ref(new moe::VkTextWidget(
                    Util::formatU32(U"{}", entry.deaths),
                    m_fontId,
                    18.0f,
                    moe::Colors::White));
            m_deathsColumn->addChild(deathsWidget);
            m_entryWidgets.push_back(deathsWidget);
        }
    }

    void ScoreDetailState::populateScoreEntries(GameManager& ctx) {
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("ScoreDetailState::onEnter: GamePlaySharedData not found");
            return;
        }

        auto entries = generateScoreEntriesFromSharedData(sharedData);
        populateScoreEntriesFrom(ctx, entries);
    }

    void ScoreDetailState::registerDebugCommand(GameManager& ctx) {
        ctx.addDebugDrawFunction(
                "ScoreDetailState Debug",
                [state = this->asRef<ScoreDetailState>(), &ctx]() mutable {
                    ImGui::Begin("ScoreDetailState Debug");

                    ImGui::Text("Number of score entries: %zu", state->m_entryWidgets.size() / 3);

                    ImGui::Separator();
                    if (ImGui::Button("Refresh Score Entries")) {
                        state->populateScoreEntries(ctx);
                        state->m_rootWidget->layout();
                    }

                    if (ImGui::Button("Mock")) {
                        auto entries = state->generateScoreEntriesMock();
                        state->populateScoreEntriesFrom(ctx, entries);
                        state->m_rootWidget->layout();
                    }

                    ImGui::End();
                });
    }

    void ScoreDetailState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering ScoreDetailState");

        m_fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (m_fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("ScoreDetailState::onEnter: failed to load font");
            return;
        }

        initWidgets(ctx);
        populateScoreEntries(ctx);

        registerDebugCommand(ctx);

        m_rootWidget->layout();
    }

    void ScoreDetailState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting ScoreDetailState");

        ctx.removeDebugDrawFunction("ScoreDetailState Debug");

        m_rootWidget.reset();
        m_baseContainer.reset();
        m_titleTextWidget.reset();
        m_scoreListContainer.reset();
        m_playerNameColumn.reset();
        m_killsColumn.reset();
        m_deathsColumn.reset();
        m_entryWidgets.clear();

        m_fontId = moe::NULL_FONT_ID;
    }

    void ScoreDetailState::onUpdate(GameManager& ctx, float deltaTime) {
        if (!m_rootWidget) {
            return;
        }

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_baseContainer->render(renderctx);
        m_titleTextWidget->render(renderctx);
        for (auto& widget: m_entryWidgets) {
            widget->render(renderctx);
        }
    }
}// namespace game::State