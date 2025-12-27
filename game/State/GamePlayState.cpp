#include "State/GamePlayState.hpp"

#include "State/ChatboxState.hpp"
#include "State/CrossHairState.hpp"
#include "State/LocalPlayerState.hpp"
#include "State/PlaygroundState.hpp"

#include "State/GamePlayData.hpp"

#include "Param.hpp"
#include "Registry.hpp"
#include "Util.hpp"

#include "FlatBuffers/Generated/Sent/receiveGamingPacket_generated.h"

#include "Localization.hpp"


namespace game::State {
    static ParamS PLAYER_NAME("gameplay.player_name", "Player", ParamScope::UserConfig);
    static ParamF GAME_EXIT_TO_MAIN_MENU_DELAY(
            "gameplay.game_exit_to_main_menu_delay",
            5.0f, ParamScope::System);

    static I18N WAITING_FOR_PLAYERS_PROMPT(
            "gameplay.waiting_for_players",
            U"Waiting for other players to be ready...");

    static I18N PURCHASE_PHASE_STARTED_PROMPT(
            "gameplay.purchase_phase_started",
            U"Purchase phase has started! Buy your equipment.");
    static I18N PURCHASE_RESULT_SUCCESS_PROMPT(
            "gameplay.purchase_result_success",
            U"Purchase successful, balance: {}");
    static I18N PURCHASE_RESULT_FAILURE_PROMPT(
            "gameplay.purchase_result_failure",
            U"Purchase failed!");

    static I18N ROUND_STARTED_PROMPT(
            "gameplay.round_started",
            U"The round has started! Good luck.");
    static I18N ROUND_ENDED_PROMPT(
            "gameplay.round_ended",
            U"Round {} has ended. Team {} won!");

    static I18N GAME_ENDED_SUMMARY_PROMPT(
            "gameplay.game_ended_summary",
            U"Game Over! Result: {}");

    static I18N GAME_ENDED_PROMPT(
            "gameplay.game_ended",
            U"The game has ended. Thank you for playing! Message from server: {}");

    static I18N PLAYER_JOINED_PROMPT(
            "gameplay.player_joined",
            U"Player '{}' has joined the game.");
    static I18N PLAYER_LEFT_PROMPT(
            "gameplay.player_left",
            U"Player '{}' has left the game.");
    static I18N ALL_PLAYERS_READY_PROMPT(
            "gameplay.all_players_ready",
            U"All players are ready. The game is starting!");

    static I18N GAME_SUMMARY_DRAW(
            "gameplay.game_summary_draw",
            U"Draw");
    static I18N GAME_SUMMARY_T_WIN(
            "gameplay.game_summary_t_win",
            U"Terrorists Win");
    static I18N GAME_SUMMARY_CT_WIN(
            "gameplay.game_summary_ct_win",
            U"Counter-Terrorists Win");

    void GamePlayState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering GamePlayState");

        moe::Logger::info("Setting up network adaptor and dispatcher...");
        ctx.network().connect();
        m_networkDispatcher = std::make_unique<game::NetworkDispatcher>(&ctx.network());

        moe::Logger::debug("Setting up gameplay shared data");
        Registry::getInstance().emplace<GamePlaySharedData>();

        initFSM(ctx);

        auto pgstate = moe::Ref(new PlaygroundState());
        this->addChildState(pgstate);

        auto playerState = moe::Ref(new LocalPlayerState());
        this->addChildState(playerState);

        auto crossHairState = moe::Ref(new CrossHairState());
        this->addChildState(crossHairState);

        m_chatboxState = moe::Ref(new ChatboxState());
        this->addChildState(m_chatboxState);
    }

    void GamePlayState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting GamePlayState");

        Registry::getInstance().remove<GamePlaySharedData>();

        m_chatboxState.reset();
    }

    void GamePlayState::initFSM(GameManager& ctx) {
        m_fsm.setContext(ctx);

        m_fsm.state(
                MatchPhase::Initializing,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();

                    sendReadySignalToServer(ctx);

                    moe::Logger::info("Sent ready signal to server, waiting for other players...");
                    moe::Logger::info("Transitioning to InWaitingRoom phase");

                    displaySystemPrompt(ctx, WAITING_FOR_PLAYERS_PROMPT.get());

                    fsm.transitionTo(MatchPhase::InWaitingRoom);
                },
                [](game::SimpleFSM<MatchPhase>& fsm) {
                    moe::Logger::info("GamePlayState FSM: Entered Initializing phase");
                });

        m_fsm.state(
                MatchPhase::InWaitingRoom,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();

                    handlePlayerJoinQuit(ctx);
                    if (!tryWaitForAllPlayersReady(ctx)) {
                        return;
                    }

                    moe::Logger::info("All players are ready, starting the game");
                    moe::Logger::info("Transitioning to GameStarting phase");

                    fsm.transitionTo(MatchPhase::GameStarting);
                },
                [](game::SimpleFSM<MatchPhase>& fsm) {
                    moe::Logger::info("GamePlayState FSM: Entered InWaitingRoom phase");
                });

        m_fsm.state(
                MatchPhase::GameStarting,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (!tryWaitForPurchasePhaseStart(ctx)) {
                        return;
                    }

                    moe::Logger::info("Transitioning to PurchasingPhase phase");

                    fsm.transitionTo(MatchPhase::PurchasingPhase);
                },
                [this](game::SimpleFSM<MatchPhase>& fsm) {
                    auto& ctx = fsm.getContext();
                    moe::Logger::info("GamePlayState FSM: Entered GameStarting phase");
                    displaySystemPrompt(ctx, ALL_PLAYERS_READY_PROMPT.get());

                    // sync time with server
                    moe::Logger::info("Synchronizing time with server...");
                    auto syncInfo = m_networkDispatcher->getLastTimeSync();
                    ctx.physics()
                            .syncTickIndex(
                                    syncInfo.lastServerTick,
                                    syncInfo.roundTripTimeMs);
                });

        m_fsm.state(
                MatchPhase::PurchasingPhase,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (!tryWaitForRoundStart(ctx)) {
                        // if still in purchasing phase, handle purchase responses
                        handlePurchaseResponse(ctx);
                        return;
                    }

                    moe::Logger::info("Transitioning to RoundInProgress phase");

                    fsm.transitionTo(MatchPhase::RoundInProgress);
                },
                [this](game::SimpleFSM<MatchPhase>& fsm) {
                    auto& ctx = fsm.getContext();
                    moe::Logger::info("GamePlayState FSM: Entered PurchasingPhase phase");

                    displaySystemPrompt(ctx, PURCHASE_PHASE_STARTED_PROMPT.get());

                    m_purchaseState = moe::Ref(new PurchaseState());
                    this->addChildState(m_purchaseState);
                },
                [this](game::SimpleFSM<MatchPhase>& fsm) {
                    moe::Logger::info("GamePlayState FSM: Exiting PurchasingPhase phase");

                    // remove purchase state
                    if (m_purchaseState) {
                        this->removeChildState(m_purchaseState);
                        m_purchaseState.reset();
                    }
                });

        m_fsm.state(
                MatchPhase::RoundInProgress,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (!tryWaitForRoundEnd(ctx)) {
                        return;
                    }

                    moe::Logger::info("Transitioning to RoundEnded phase");

                    fsm.transitionTo(MatchPhase::RoundEnded);
                },
                [this](game::SimpleFSM<MatchPhase>& fsm) {
                    auto& ctx = fsm.getContext();
                    moe::Logger::info("GamePlayState FSM: Entered RoundInProgress phase");
                    moe::Logger::info("Round started");

                    displaySystemPrompt(ctx, ROUND_STARTED_PROMPT.get());
                });

        m_fsm.state(
                MatchPhase::RoundEnded,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (tryWaitForGameEnd(ctx)) {
                        moe::Logger::info("Transitioning to GameEnded phase");

                        fsm.transitionTo(MatchPhase::GameEnded);
                        return;
                    }

                    if (!tryWaitForPurchasePhaseStart(ctx)) {
                        return;
                    }

                    moe::Logger::info("Transitioning to PurchasingPhase phase");

                    fsm.transitionTo(MatchPhase::PurchasingPhase);
                },
                [](game::SimpleFSM<MatchPhase>& fsm) {
                    moe::Logger::info("GamePlayState FSM: Entered RoundEnded phase");
                    moe::Logger::info("Round ended");
                });

        m_fsm.state(
                MatchPhase::GameEnded,
                [this](game::SimpleFSM<MatchPhase>& fsm, float dt) {
                    static float timeAccum = 0.0f;
                    timeAccum += dt;

                    if (timeAccum < GAME_EXIT_TO_MAIN_MENU_DELAY.get()) {
                        return;
                    }
                    timeAccum = 0.0f;// reset for next time

                    auto& ctx = fsm.getContext();

                    moe::Logger::info("Exiting to main menu...");

                    // exit to main menu
                    ctx.queueFree(this->intoRef());
                },
                [](game::SimpleFSM<MatchPhase>& fsm) {
                    moe::Logger::info("GamePlayState FSM: Entered GameEnded phase");
                    moe::Logger::info("Game ended");
                });
    }

    void GamePlayState::displaySystemPrompt(GameManager& ctx, moe::U32StringView prompt, const moe::Color& color) {
        auto chatboxState = m_chatboxState;
        if (!chatboxState) {
            moe::Logger::warn("GamePlayState::displaySystemPrompt: ChatboxState not found");
            return;
        }

        chatboxState->addChatMessage(
                U"System", prompt,
                color);
    }

    void GamePlayState::displaySystemError(GameManager& ctx, moe::U32StringView errorMsg) {
        displaySystemPrompt(ctx, errorMsg, moe::Colors::Red);
    }

    void GamePlayState::sendReadySignalToServer(GameManager& ctx) {
        auto& network = ctx.network();

        auto builder = flatbuffers::FlatBufferBuilder();
        auto playerInfo = myu::net::CreatePlayerInfo(
                builder,
                builder.CreateString(PLAYER_NAME.get()),
                true);

        auto timePack = game::Util::getTimePack();
        auto header = myu::net::CreatePacketHeader(
                builder,
                timePack.physicsTick,
                timePack.currentTimeMillis);

        auto message = myu::net::CreateNetMessage(
                builder,
                header,
                myu::net::PacketUnion::PlayerInfo,
                playerInfo.Union());

        builder.Finish(message);

        auto dataSpan = builder.GetBufferSpan();
        network.sendData(dataSpan, true);
    }

    bool GamePlayState::tryWaitForAllPlayersReady(GameManager& ctx) {
        auto& gameStartQueue = m_networkDispatcher->getQueues().queueGameStartedEvent;
        if (gameStartQueue.empty()) {
            // keep waiting
            return false;
        }

        // all players ready
        const auto& event = gameStartQueue.front();
        // todo: process event data if needed
        for (const auto& player: event.players->players) {
            moe::Logger::info(
                    "Player '{}'(id: {}) is ready, team {}",
                    player->name,
                    player->tempId,
                    player->team == moe::net::PlayerTeam::TEAM_CT ? "CT" : "T");
        }
        moe::Logger::info("Who am I: '{}'(id: {})", event.players->whoami->name, event.players->whoami->tempId);

        auto* gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        gamePlaySharedData->playerTempId = event.players->whoami->tempId;

        gameStartQueue.pop_front();

        return true;
    }

    void GamePlayState::handlePlayerJoinQuit(GameManager& ctx) {
        auto& joinQueue = m_networkDispatcher->getQueues().queuePlayerJoinedEvent;
        auto& quitQueue = m_networkDispatcher->getQueues().queuePlayerLeftEvent;

        while (!joinQueue.empty()) {
            const auto& event = joinQueue.front();
            moe::Logger::info("Player joined: {}, current player count: {}", event.name, event.playerCount);

            displaySystemPrompt(ctx, Util::formatU32(PLAYER_JOINED_PROMPT.get(), event.name));
            joinQueue.pop_front();
        }

        while (!quitQueue.empty()) {
            const auto& event = quitQueue.front();
            moe::Logger::info("Player left: {}, current player count: {}", event.name, event.playerCount);
            displaySystemPrompt(ctx, Util::formatU32(PLAYER_LEFT_PROMPT.get(), event.name));
            quitQueue.pop_front();
        }
    }

    bool GamePlayState::tryWaitForPurchasePhaseStart(GameManager& ctx) {
        auto& purchasePhaseStartQueue = m_networkDispatcher->getQueues().queueRoundPurchaseStartedEvent;
        if (purchasePhaseStartQueue.empty()) {
            // keep waiting
            return false;
        }

        // purchase phase started
        const auto& event = purchasePhaseStartQueue.front();
        // todo: process event data if needed

        purchasePhaseStartQueue.pop_front();
        return true;
    }

    void GamePlayState::handlePurchaseResponse(GameManager& ctx) {
        auto& purchaseResponseQueue = m_networkDispatcher->getQueues().queuePurchaseEvent;
        while (!purchaseResponseQueue.empty()) {
            const auto& event = purchaseResponseQueue.front();

            if (event.success) {
                moe::Logger::info(
                        "Purchase successful: balance after purchase: {}",
                        event.balance);

                displaySystemPrompt(
                        ctx,
                        Util::formatU32(
                                PURCHASE_RESULT_SUCCESS_PROMPT.get(),
                                event.balance),
                        moe::Colors::Green);
            } else {
                moe::Logger::info("Purchase failed");
                displaySystemError(ctx, PURCHASE_RESULT_FAILURE_PROMPT.get());
            }
            purchaseResponseQueue.pop_front();
        }
    }

    bool GamePlayState::tryWaitForRoundStart(GameManager& ctx) {
        auto& roundStartQueue = m_networkDispatcher->getQueues().queueRoundStartedEvent;
        if (roundStartQueue.empty()) {
            // keep waiting
            return false;
        }

        // round started
        const auto& event = roundStartQueue.front();
        // todo: process event data if needed

        roundStartQueue.pop_front();

        return true;
    }

    bool GamePlayState::tryWaitForRoundEnd(GameManager& ctx) {
        auto& roundEndQueue = m_networkDispatcher->getQueues().queueRoundEndedEvent;
        if (roundEndQueue.empty()) {
            // keep waiting
            return false;
        }

        // round ended
        const auto& event = roundEndQueue.front();
        moe::Logger::info("Round ended: winning team: {}, round number: {}",
                          event.winningTeam == moe::net::PlayerTeam::TEAM_CT ? "CT" : "T",
                          event.roundNumber);

        displaySystemPrompt(ctx,
                            Util::formatU32(
                                    ROUND_ENDED_PROMPT.get(), event.roundNumber,
                                    event.winningTeam == moe::net::PlayerTeam::TEAM_CT
                                            ? "CT"
                                            : "T"));

        roundEndQueue.pop_front();

        return true;
    }

    bool GamePlayState::tryWaitForGameEnd(GameManager& ctx) {
        auto& gameEndQueue = m_networkDispatcher->getQueues().queueGameEndedEvent;
        if (gameEndQueue.empty()) {
            // keep waiting
            return false;
        }

        // game ended
        const auto& event = gameEndQueue.front();
        moe::Logger::info("Game ended: reason: {}",
                          event.reason);

        moe::U32String summary;
        switch (event.winner) {
            case moe::net::PlayerTeam::TEAM_NONE:
                summary = GAME_SUMMARY_DRAW.get();
                break;
            case moe::net::PlayerTeam::TEAM_T:
                summary = GAME_SUMMARY_T_WIN.get();
                break;
            case moe::net::PlayerTeam::TEAM_CT:
                summary = GAME_SUMMARY_CT_WIN.get();
                break;
        }

        displaySystemPrompt(
                ctx,
                Util::formatU32(
                        GAME_ENDED_SUMMARY_PROMPT.get(),
                        utf8::utf32to8(summary)));
        displaySystemPrompt(ctx, Util::formatU32(GAME_ENDED_PROMPT.get(), event.reason));

        gameEndQueue.pop_front();

        return true;
    }

    void GamePlayState::onUpdate(GameManager& ctx, float deltaTime) {
        m_networkDispatcher->dispatchReceiveData();

        m_fsm.update(deltaTime);
    }
}// namespace game::State