#include "State/GamePlayState.hpp"

#include "State/LocalPlayerState.hpp"
#include "State/PlaygroundState.hpp"

#include "Param.hpp"
#include "Util.hpp"

#include "Flatbuffers/Generated/Sent/receiveGamingPacket_generated.h"


namespace game::State {
    static ParamS PLAYER_NAME("gameplay.player_name", "Player", ParamScope::UserConfig);

    void GamePlayState::onEnter(GameManager& ctx) {
        m_networkDispatcher = std::make_unique<game::NetworkDispatcher>(&ctx.network());

        auto pgstate = moe::Ref(new PlaygroundState());
        this->addChildState(pgstate);

        auto playerState = moe::Ref(new LocalPlayerState());
        this->addChildState(playerState);
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
        // todo: sync time with server

        gameStartQueue.pop_front();

        return true;
    }

    void GamePlayState::handlePlayerJoinQuit(GameManager& ctx) {
        auto& joinQueue = m_networkDispatcher->getQueues().queuePlayerJoinedEvent;
        auto& quitQueue = m_networkDispatcher->getQueues().queuePlayerLeftEvent;

        while (!joinQueue.empty()) {
            const auto& event = joinQueue.front();
            moe::Logger::info("Player joined: {}, current player count: {}", event.name, event.playerCount);
            joinQueue.pop_front();
        }

        while (!quitQueue.empty()) {
            const auto& event = quitQueue.front();
            moe::Logger::info("Player left: {}, current player count: {}", event.name, event.playerCount);
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

    void GamePlayState::updateFSM(GameManager& ctx, float deltaTime) {
        switch (m_fsmState) {
            case FSMState::Initializing: {
                // initial setup
                moe::Logger::info("GamePlayState initializing");

                m_fsmState = FSMState::InWaitingRoom;
                sendReadySignalToServer(ctx);
                break;
            }
            case FSMState::InWaitingRoom: {
                // wait for players to join
                handlePlayerJoinQuit(ctx);
                if (tryWaitForAllPlayersReady(ctx)) {
                    moe::Logger::info("All players are ready, starting the game");
                    m_fsmState = FSMState::GameStarting;
                }
                break;
            }
            case FSMState::GameStarting: {
                // game is starting, waiting for purchase phase
                if (tryWaitForPurchasePhaseStart(ctx)) {
                    moe::Logger::info("Purchase phase started");
                    m_fsmState = FSMState::PurchasingPhase;
                }
                break;
            }
            case FSMState::PurchasingPhase: {
                // allow players to purchase equipment
                if (tryWaitForRoundStart(ctx)) {
                    moe::Logger::info("Round started");
                    m_fsmState = FSMState::RoundInProgress;
                }
                // todo: implement purchase logic
                break;
            }
            case FSMState::RoundInProgress: {
                // main gameplay
                break;
            }
            case FSMState::RoundEnded: {
                // show round results
                break;
            }
            case FSMState::GameEnded: {
                // show game results, return to lobby
                break;
            }
        }
    }

    void GamePlayState::onUpdate(GameManager& ctx, float deltaTime) {
        m_networkDispatcher->dispatchReceiveData();

        updateFSM(ctx, deltaTime);
    }
}// namespace game::State