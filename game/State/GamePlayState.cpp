#include "State/GamePlayState.hpp"

#include "State/ChatboxState.hpp"
#include "State/LocalPlayerState.hpp"
#include "State/PlaygroundState.hpp"

#include "Param.hpp"
#include "Util.hpp"

#include "FlatBuffers/Generated/Sent/receiveGamingPacket_generated.h"


namespace game::State {
    static ParamS PLAYER_NAME("gameplay.player_name", "Player", ParamScope::UserConfig);

    void GamePlayState::onEnter(GameManager& ctx) {
        m_networkDispatcher = std::make_unique<game::NetworkDispatcher>(&ctx.network());

        initFSM(ctx);

        auto pgstate = moe::Ref(new PlaygroundState());
        this->addChildState(pgstate);

        auto playerState = moe::Ref(new LocalPlayerState());
        this->addChildState(playerState);

        auto chatboxState = moe::Ref(new ChatboxState());
        this->addChildState(chatboxState);
    }

    void GamePlayState::initFSM(GameManager& ctx) {
        m_fsm.setContext(ctx);

        m_fsm.state(
                MatchPhase::Initializing,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    sendReadySignalToServer(fsm.getContext());

                    moe::Logger::info("Sent ready signal to server, waiting for other players...");
                    moe::Logger::info("Transitioning to InWaitingRoom phase");

                    fsm.transitionTo(MatchPhase::InWaitingRoom);
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
                });

        m_fsm.state(
                MatchPhase::GameStarting,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (!tryWaitForPurchasePhaseStart(ctx)) {
                        return;
                    }

                    moe::Logger::info("Purchase phase started");
                    moe::Logger::info("Transitioning to PurchasingPhase phase");

                    fsm.transitionTo(MatchPhase::PurchasingPhase);
                });

        m_fsm.state(
                MatchPhase::PurchasingPhase,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    auto& ctx = fsm.getContext();
                    if (!tryWaitForRoundStart(ctx)) {
                        return;
                    }

                    moe::Logger::info("Round started");
                    moe::Logger::info("Transitioning to RoundInProgress phase");

                    fsm.transitionTo(MatchPhase::RoundInProgress);
                });

        m_fsm.state(
                MatchPhase::RoundInProgress,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    // todo
                });

        m_fsm.state(
                MatchPhase::RoundEnded,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    // todo
                });

        m_fsm.state(
                MatchPhase::GameEnded,
                [this](game::SimpleFSM<MatchPhase>& fsm, float) {
                    // todo
                });
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
        for (const auto& player: event.players->players) {
            moe::Logger::info(
                    "Player '{}'(id: {}) is ready, team {}",
                    player->name,
                    player->tempId,
                    player->team == moe::net::PlayerTeam::TEAM_CT ? "CT" : "T");
        }
        moe::Logger::info("Who am I: '{}'(id: {})", event.players->whoami->name, event.players->whoami->tempId);

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

    void GamePlayState::onUpdate(GameManager& ctx, float deltaTime) {
        m_networkDispatcher->dispatchReceiveData();

        m_fsm.update(deltaTime);
    }
}// namespace game::State