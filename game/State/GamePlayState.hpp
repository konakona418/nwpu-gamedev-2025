#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

#include "NetworkDispatcher.hpp"
#include "SimpleFSM.hpp"

namespace game::State {
    enum class MatchPhase {
        Initializing,

        InWaitingRoom,

        GameStarting,

        PurchasingPhase,
        RoundInProgress,
        RoundEnded,

        GameEnded,
    };

    struct GamePlayState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "GamePlayState";
        }

        void onEnter(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        // void onExit(GameManager& ctx) override;

    private:
        moe::UniquePtr<NetworkDispatcher> m_networkDispatcher{nullptr};
        game::SimpleFSM<MatchPhase, MatchPhase::Initializing> m_fsm{};

        void initFSM(GameManager& ctx);

        void sendReadySignalToServer(GameManager& ctx);

        bool tryWaitForAllPlayersReady(GameManager& ctx);
        void handlePlayerJoinQuit(GameManager& ctx);

        bool tryWaitForPurchasePhaseStart(GameManager& ctx);
        bool tryWaitForRoundStart(GameManager& ctx);
    };
}// namespace game::State