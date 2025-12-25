#include "State/GamePlayState.hpp"

#include "State/LocalPlayerState.hpp"
#include "State/PlaygroundState.hpp"


namespace game::State {
    void GamePlayState::onEnter(GameManager& ctx) {
        auto pgstate = moe::Ref(new PlaygroundState());
        this->addChildState(pgstate);

        auto playerState = moe::Ref(new LocalPlayerState());
        this->addChildState(playerState);
    }
}// namespace game::State