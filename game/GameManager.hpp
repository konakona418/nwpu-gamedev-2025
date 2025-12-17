#pragma once

#include "GameState.hpp"

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    struct App;

    struct GameManager {
    public:
        GameManager(App* app) : m_app(app) {}

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
            for (auto& state: m_gameStateStack) {
                state->_onUpdate(*this, deltaTimeSecs);
            }
        }

        void physicsUpdate(float deltaTimeSecs) {
            if (m_gameStateStack.empty()) return;
            for (auto& state: m_gameStateStack) {
                state->_onPhysicsUpdate(*this, deltaTimeSecs);
            }
        }

        moe::VulkanEngine& renderer();
        moe::PhysicsEngine& physics();
        moe::AudioEngineInterface& audio();

    private:
        moe::Vector<moe::Ref<game::GameState>> m_gameStateStack;
        App* m_app = nullptr;
    };
}// namespace game