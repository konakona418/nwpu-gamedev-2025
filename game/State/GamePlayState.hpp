#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

#include "NetworkDispatcher.hpp"

namespace game::State {
    struct GamePlayState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "GamePlayState";
        }

        void onEnter(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        // void onExit(GameManager& ctx) override;

    private:
        enum class FSMState {
            Initializing,

            InWaitingRoom,

            GameStarting,

            PurchasingPhase,
            RoundInProgress,
            RoundEnded,

            GameEnded,
        };

        moe::UniquePtr<NetworkDispatcher> m_networkDispatcher{nullptr};
        FSMState m_fsmState{FSMState::Initializing};

        void updateFSM(GameManager& ctx, float deltaTime);

        void sendReadySignalToServer(GameManager& ctx);

        bool tryWaitForAllPlayersReady(GameManager& ctx);
        void handlePlayerJoinQuit(GameManager& ctx);

        bool tryWaitForPurchasePhaseStart(GameManager& ctx);
        bool tryWaitForRoundStart(GameManager& ctx);
    };
}// namespace game::State