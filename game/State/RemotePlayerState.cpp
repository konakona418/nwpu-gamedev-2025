#include "State/RemotePlayerState.hpp"

namespace game::State {
    void RemotePlayerState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering RemotePlayerState");
    }

    void RemotePlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        // WIP
    }

    void RemotePlayerState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting RemotePlayerState");
    }
}// namespace game::State