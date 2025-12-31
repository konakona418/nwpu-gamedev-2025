#include "State/GamePlayState.hpp"

#include "State/ChatboxState.hpp"
#include "State/CrossHairState.hpp"
#include "State/LocalPlayerState.hpp"
#include "State/PlaygroundState.hpp"
#include "State/RemotePlayerState.hpp"


#include "State/GamePlayData.hpp"

#include "Param.hpp"
#include "Registry.hpp"
#include "Util.hpp"

#include "FlatBuffers/Generated/Sent/receiveGamingPacket_generated.h"

#include "Localization.hpp"
#include "imgui.h"


namespace game::State {
    static ParamS PLAYER_NAME("gameplay.player_name", "Player", ParamScope::UserConfig);
    static ParamF GAME_EXIT_TO_MAIN_MENU_DELAY(
            "gameplay.game_exit_to_main_menu_delay",
            5.0f, ParamScope::System);
    static ParamF GUNSHOT_SOUND_DURATION(
            "gameplay.gunshot_sound_duration",
            2.0f, ParamScope::System);
    static ParamI MAX_SIMULTANEOUS_GUNSHOTS(
            "gameplay.max_simultaneous_gunshots",
            32, ParamScope::System);

    static I18N WAITING_FOR_CONNECTION_PROMPT(
            "gameplay.waiting_for_connection",
            U"Connecting to server...");

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
    static I18N PLAYER_KILLED_PROMPT(
            "gameplay.player_killed",
            U"You were killed by '{}', weapon: {}.");
    static I18N PLAYER_KILLED_OTHER_PROMPT(
            "gameplay.player_killed_other",
            U"Player '{}' was killed by '{}', weapon: {}.");
    static I18N BOMB_PLANTED_PROMPT(
            "gameplay.bomb_planted",
            U"Bomb has been planted at Site {} by {}!");
    static I18N BOMB_DEFUSED_PROMPT(
            "gameplay.bomb_defused",
            U"Bomb has been defused by {}!");
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

#define _GAME_WEAPON_ITEM_NAME_TO_ENUM_MAP_XXX()   \
    X(U"Glock", ::moe::net::Weapon::GLOCK)         \
    X(U"USP", ::moe::net::Weapon::USP)             \
    X(U"Desert Eagle", ::moe::net::Weapon::DEAGLE) \
    X(U"AK47", ::moe::net::Weapon::AK47)           \
    X(U"M4A1", ::moe::net::Weapon::M4A1)

    moe::U32StringView weaponNameFromNetEnum(::moe::net::Weapon weapon) {
#define X(name, enumVal) \
    case enumVal:        \
        return name;

        switch (weapon) {
            _GAME_WEAPON_ITEM_NAME_TO_ENUM_MAP_XXX()
            case moe::net::Weapon::WEAPON_NONE:
                break;
        }

#undef X

        return U"No weapon";
    }

#undef _GAME_WEAPON_ITEM_NAME_TO_ENUM_MAP_XXX

    void GamePlayState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering GamePlayState");

        moe::Logger::info("Setting up network adaptor and dispatcher...");
        ctx.network().connect();
        m_networkDispatcher = std::make_unique<game::NetworkDispatcher>(&ctx.network());

        auto gunshotData = m_gunshotSoundLoader.generate();
        if (gunshotData) {
            m_gunshotSoundProvider = moe::makeRef<moe::StaticOggProvider>(gunshotData.value());
        } else {
            moe::Logger::error("GamePlayState::onEnter: failed to load gunshot sound data");
        }

        // preload audio sources for gunshots
        for (int i = 0; i < MAX_SIMULTANEOUS_GUNSHOTS.get(); i++) {
            auto audioInterface = ctx.audio();
            auto audioSource = audioInterface.createAudioSource();
            audioInterface.loadAudioSource(audioSource, m_gunshotSoundProvider, false);
            m_activeGunshots.push_back(audioSource);
        }

        moe::Logger::debug("Setting up gameplay shared data");
        Registry::getInstance().emplace<GamePlaySharedData>();
        Registry::getInstance().get<GamePlaySharedData>()->networkDispatcher = m_networkDispatcher.get();

        initFSM(ctx);

        auto pgstate = moe::Ref(new PlaygroundState());
        this->addChildState(pgstate);

        auto crossHairState = moe::Ref(new CrossHairState());
        this->addChildState(crossHairState);

        m_chatboxState = moe::Ref(new ChatboxState());
        this->addChildState(m_chatboxState);

        m_localPlayerState = moe::Ref(new LocalPlayerState());
        this->addChildState(m_localPlayerState);
    }

    void GamePlayState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting GamePlayState");

        Registry::getInstance().remove<GamePlaySharedData>();

        m_chatboxState.reset();

        ctx.network().shutdown();
    }

    void GamePlayState::initFSM(GameManager& ctx) {
        m_fsm.setContext(ctx);

        m_fsm.state(
                MatchPhase::Initializing,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (!ctx.network().isConnected()) {
                        // wait until connected
                        return;
                    }

                    sendReadySignalToServer(ctx);

                    moe::Logger::info("Sent ready signal to server, waiting for other players...");
                    moe::Logger::info("Transitioning to InWaitingRoom phase");

                    displaySystemPrompt(ctx, WAITING_FOR_PLAYERS_PROMPT.get());

                    fsm.transitionTo(MatchPhase::InWaitingRoom);
                },
                [this](game::SimpleFSM<MatchPhase>& fsm) {
                    moe::Logger::info("GamePlayState FSM: Entered Initializing phase");
                    displaySystemPrompt(
                            fsm.getContext(),
                            WAITING_FOR_CONNECTION_PROMPT.get());
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

                    // if still in purchasing phase, handle purchase responses
                    handlePurchaseResponse(ctx);
                    if (!tryWaitForRoundStart(ctx)) {
                        return;
                    }

                    moe::Logger::info("Transitioning to RoundInProgress phase");

                    fsm.transitionTo(MatchPhase::RoundInProgress);
                },
                [this](game::SimpleFSM<MatchPhase>& fsm) {
                    auto& ctx = fsm.getContext();
                    moe::Logger::info("GamePlayState FSM: Entered PurchasingPhase phase");

                    displaySystemPrompt(ctx, PURCHASE_PHASE_STARTED_PROMPT.get());

                    if (m_localPlayerState) {
                        moe::Logger::info("Marking LocalPlayerState as valid");
                        m_localPlayerState->setValid(true);// mark as valid
                    } else {
                        moe::Logger::error("GamePlayState FSM: LocalPlayerState not found when entering PurchasingPhase");
                    }

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
                [this](game::SimpleFSM<MatchPhase>& fsm, float deltaTime) {
                    auto& ctx = fsm.getContext();

                    // handle in-round events
                    handlePlayerDeaths(ctx);
                    handleBombEvents(ctx);
                    handleGunshotEvents(ctx, deltaTime);

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

                    auto gamePlaySharedData = Registry::getInstance().get<GamePlaySharedData>();
                    if (gamePlaySharedData) {
                        gamePlaySharedData->isBombPlanted = false;// reset bomb planted state
                    }
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

        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        gamePlaySharedData->playerTempId = event.players->whoami->tempId;
        gamePlaySharedData->playerTeam =
                event.players->whoami->team == moe::net::PlayerTeam::TEAM_CT
                        ? GamePlayerTeam::CT
                        : GamePlayerTeam::T;

        // register player update buffers, populate player info map
        for (const auto& player: event.players->players) {
            m_networkDispatcher->registerPlayerUpdateBuffer(player->tempId);

            GamePlayerTeam team = GamePlayerTeam::None;
            switch (player->team) {
                case moe::net::PlayerTeam::TEAM_T:
                    team = GamePlayerTeam::T;
                    break;
                case moe::net::PlayerTeam::TEAM_CT:
                    team = GamePlayerTeam::CT;
                    break;
                default:
                    team = GamePlayerTeam::None;
                    break;
            }
            gamePlaySharedData->playersByTempId[player->tempId] = GamePlayer{
                    .tempId = player->tempId,
                    .name = utf8::utf8to32(player->name),
                    .team = team,
            };

            if (player->tempId != gamePlaySharedData->playerTempId) {
                // create remote player state
                auto remotePlayerState = moe::Ref(new RemotePlayerState(player->tempId));
                this->addChildState(remotePlayerState);
            }
        }

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
        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        gamePlaySharedData->playerBalance = event.balance;
        moe::Logger::info("Purchase phase player balance: {}", event.balance);

        purchasePhaseStartQueue.pop_front();
        return true;
    }

    static WeaponItems weaponNetEnumToItem(::moe::net::Weapon weapon) {
#define X(_1, enumVal, _2, moeEnum) \
    case moeEnum:                   \
        return enumVal;

        switch (weapon) {
            _GAME_STATE_WEAPON_ITEM_NAME_ENUM_MAP_XXX()
            case moe::net::Weapon::WEAPON_NONE:
                break;
        }
#undef X
        return WeaponItems::None;
    }

    static moe::StringView weaponNetEnumToStringName(::moe::net::Weapon weapon) {
#define X(name, _1, _2, moeEnum) \
    case moeEnum:                \
        return name;

        switch (weapon) {
            _GAME_STATE_WEAPON_ITEM_NAME_ENUM_MAP_XXX()
            case moe::net::Weapon::WEAPON_NONE:
                break;
        }
#undef X
        return "N/A";
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

                auto gamePlaySharedData =
                        Registry::getInstance().get<GamePlaySharedData>();
                gamePlaySharedData->playerBalance = event.balance;// update balance
                gamePlaySharedData->playerPrimaryWeapon = weaponNetEnumToItem(event.primaryWeapon);
                gamePlaySharedData->playerSecondaryWeapon = weaponNetEnumToItem(event.secondaryWeapon);

                moe::Logger::info(
                        "Updated player weapons: primary: {}, secondary: {}",
                        weaponNetEnumToStringName(event.primaryWeapon),
                        weaponNetEnumToStringName(event.secondaryWeapon));
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

    void GamePlayState::handlePlayerDeaths(GameManager& ctx) {
        auto& playerKilledQueue = m_networkDispatcher->getQueues().queuePlayerKilledEvent;
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("GamePlayState::handlePlayerDeaths: GamePlaySharedData not found");
            return;
        }

        while (!playerKilledQueue.empty()) {
            const auto& event = playerKilledQueue.front();

            if (event.victimTempId == sharedData->playerTempId) {
                moe::Logger::info("Player was killed by player id: {}", event.killerTempId);
                displaySystemPrompt(
                        ctx,
                        Util::formatU32(
                                PLAYER_KILLED_PROMPT.get(),
                                utf8::utf32to8(sharedData->playersByTempId[event.killerTempId].name),
                                utf8::utf32to8(weaponNameFromNetEnum(event.weapon))));

                moe::Logger::info("Invalidating LocalPlayerState due to player death");

                if (m_localPlayerState) {
                    m_localPlayerState->setValid(false);// mark as invalid
                } else {
                    // somehow after player is dead, his rivals still kill him again
                    // this is about server logic, but we just log a warning here
                    moe::Logger::warn("LocalPlayerState not found when handling player death");
                }
            } else {
                moe::Logger::info("Player id: {} killed player id: {}", event.killerTempId, event.victimTempId);
                displaySystemPrompt(
                        ctx,
                        Util::formatU32(
                                PLAYER_KILLED_OTHER_PROMPT.get(),
                                utf8::utf32to8(sharedData->playersByTempId[event.victimTempId].name),
                                utf8::utf32to8(sharedData->playersByTempId[event.killerTempId].name),
                                utf8::utf32to8(weaponNameFromNetEnum(event.weapon))));
            }

            playerKilledQueue.pop_front();
        }
    }

    void GamePlayState::handleBombEvents(GameManager& ctx) {
        auto& bombPlantedQueue = m_networkDispatcher->getQueues().queueBombPlantedEvent;
        auto& bombDefusedQueue = m_networkDispatcher->getQueues().queueBombDefusedEvent;

        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("GamePlayState::handleBombEvents: GamePlaySharedData not found");
            return;
        }

        while (!bombPlantedQueue.empty()) {
            const auto& event = bombPlantedQueue.front();
            auto playerName = utf8::utf32to8(sharedData->playersByTempId[event.planterTempId].name);
            moe::Logger::info("Bomb planted at site id: {} by player id: {} ({})", event.bombSite, event.planterTempId, playerName);

            displaySystemPrompt(
                    ctx,
                    Util::formatU32(
                            BOMB_PLANTED_PROMPT.get(),
                            event.bombSite == 0 ? "A" : "B",
                            playerName),
                    moe::Colors::Yellow);

            sharedData->isBombPlanted = true;// mark bomb as planted

            bombPlantedQueue.pop_front();
        }

        while (!bombDefusedQueue.empty()) {
            const auto& event = bombDefusedQueue.front();
            auto playerName = utf8::utf32to8(sharedData->playersByTempId[event.defuserTempId].name);
            moe::Logger::info("Bomb defused by player id: {} ({})", event.defuserTempId, playerName);

            displaySystemPrompt(
                    ctx,
                    Util::formatU32(
                            BOMB_DEFUSED_PROMPT.get(),
                            playerName),
                    moe::Colors::Yellow);

            sharedData->isBombPlanted = false;// mark bomb as defused

            bombDefusedQueue.pop_front();
        }
    }

    void GamePlayState::handleGunshotEvents(GameManager& ctx, float deltaTime) {
        auto& gunshotQueue = m_networkDispatcher->getQueues().queuePlayerOpenFireEvent;
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("GamePlayState::handleGunshotEvents: GamePlaySharedData not found");
            return;
        }

        while (!gunshotQueue.empty()) {
            const auto& event = gunshotQueue.front();

            // play gunshot sound
            if (m_gunshotSoundProvider) {
                moe::Logger::debug("Playing gunshot sound at position ({}, {}, {})",
                                   event.posX, event.posY, event.posZ);

                auto audioInterface = ctx.audio();
                auto audioSource = m_activeGunshots.front();
                m_activeGunshots.pop_front();

                audioInterface.setAudioSourcePosition(
                        audioSource,
                        event.posX, event.posY, event.posZ);
                audioInterface.playAudioSource(audioSource);

                m_activeGunshots.push_back(audioSource);
            } else {
                moe::Logger::warn("GamePlayState::handleGunshotEvents: gunshot sound provider is null");
            }

            gunshotQueue.pop_front();
        }
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