#pragma once

#include "GameState.hpp"

namespace game {
    struct GameManager {
    public:
        void pushState(moe::Ref<GameState> state) {
            m_gameStateStack.push_back(state);
            moe::Logger::debug("state {} pushed", state->getName());
            state->_onEnter(*this);
        }

        void popState() {
            if (m_gameStateStack.empty()) return;
            auto state = m_gameStateStack.back();
            moe::Logger::debug("state {} popped", state->getName());
            state->_onExit(*this);

            m_gameStateStack.pop_back();
        }

        void update(float deltaTimeSecs) {
            if (m_gameStateStack.empty()) return;
            m_gameStateStack.back()->_onUpdate(*this, deltaTimeSecs);
        }

        void physicsUpdate(float deltaTimeSecs) {
            if (m_gameStateStack.empty()) return;
            m_gameStateStack.back()->_onPhysicsUpdate(*this, deltaTimeSecs);
        }

    private:
        moe::Vector<moe::Ref<game::GameState>> m_gameStateStack;
    };
}// namespace game