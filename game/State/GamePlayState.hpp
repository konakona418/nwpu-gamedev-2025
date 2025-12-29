#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"

#include "NetworkDispatcher.hpp"
#include "SimpleFSM.hpp"

#include "State/ChatboxState.hpp"
#include "State/LocalPlayerState.hpp"
#include "State/PurchaseState.hpp"

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
        void onExit(GameManager& ctx) override;

    private:
        moe::UniquePtr<NetworkDispatcher> m_networkDispatcher{nullptr};
        game::SimpleFSM<MatchPhase, MatchPhase::Initializing> m_fsm{};

        moe::Ref<State::ChatboxState> m_chatboxState{nullptr};

        // temporary purchase state
        moe::Ref<State::PurchaseState> m_purchaseState{nullptr};
        moe::Ref<State::LocalPlayerState> m_localPlayerState{nullptr};

        void initFSM(GameManager& ctx);

        void displaySystemPrompt(GameManager& ctx, moe::U32StringView prompt, const moe::Color& color = moe::Colors::Yellow);
        void displaySystemError(GameManager& ctx, moe::U32StringView errorMsg);

        void sendReadySignalToServer(GameManager& ctx);

        bool tryWaitForAllPlayersReady(GameManager& ctx);
        void handlePlayerJoinQuit(GameManager& ctx);

        bool tryWaitForPurchasePhaseStart(GameManager& ctx);
        void handlePurchaseResponse(GameManager& ctx);

        bool tryWaitForRoundStart(GameManager& ctx);
        void handlePlayerDeaths(GameManager& ctx);
        bool tryWaitForRoundEnd(GameManager& ctx);

        bool tryWaitForGameEnd(GameManager& ctx);
    };
}// namespace game::State