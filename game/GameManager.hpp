#pragma once

#include "GameState.hpp"

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    struct App;
    struct Input;

    struct GameManager {
    public:
        GameManager(App* app) : m_app(app) {}

        void pushState(moe::Ref<GameState> state) {
            std::lock_guard<std::mutex> lock(m_actionMutex);
            m_pendingActions.push_back({ActionType::Push, state});
        }

        void popState() {
            std::lock_guard<std::mutex> lock(m_actionMutex);
            m_pendingActions.push_back({ActionType::Pop, moe::Ref<GameState>(nullptr)});
        }

        bool processPendingActions() {
            moe::Vector<PendingAction> actionsCopy;
            {
                std::lock_guard<std::mutex> lock(m_actionMutex);
                if (m_pendingActions.empty()) {
                    return false;
                }
                std::swap(actionsCopy, m_pendingActions);
            }

            for (auto& action: actionsCopy) {
                switch (action.type) {
                    case ActionType::Push:
                        m_gameStateStack.push_back(action.state);
                        action.state->_onEnter(*this);
                        break;
                    case ActionType::Pop:
                        if (!m_gameStateStack.empty()) {
                            m_gameStateStack.back()->_onExit(*this);
                            m_gameStateStack.pop_back();
                        }
                        break;
                }
            }
            m_pendingActions.clear();

            return true;
        }

        void update(float deltaTimeSecs) {
            bool diff = processPendingActions();

            if (m_gameStateStack.empty()) return;

            for (auto& state: m_gameStateStack) {
                state->_onUpdate(*this, deltaTimeSecs);
            }

            if (diff) {
                std::lock_guard<std::mutex> lock(m_gameStateStackCopyMutex);
                m_gameStateStackCopy = m_gameStateStack;
            }
        }

        void physicsUpdate(float deltaTimeSecs) {
            moe::Vector<moe::Ref<game::GameState>> stackCopy;
            {
                std::lock_guard<std::mutex> lock(m_gameStateStackCopyMutex);
                if (m_gameStateStackCopy.empty()) return;
                stackCopy = m_gameStateStackCopy;
            }

            for (auto& state: stackCopy) {
                state->_onPhysicsUpdate(*this, deltaTimeSecs);
            }
        }

        moe::VulkanEngine& renderer();
        moe::PhysicsEngine& physics();
        moe::AudioEngineInterface& audio();
        game::Input& input();

    private:
        enum class ActionType {
            Push,
            Pop,
        };

        struct PendingAction {
            ActionType type;
            moe::Ref<GameState> state;
        };

        moe::Vector<PendingAction> m_pendingActions;
        std::mutex m_actionMutex;

        // ! fixme: this is not thread safe, when reallocating the vector, the reference will be invalid
        moe::Vector<moe::Ref<game::GameState>> m_gameStateStack;

        moe::Vector<moe::Ref<game::GameState>> m_gameStateStackCopy;
        std::mutex m_gameStateStackCopyMutex;

        App* m_app = nullptr;
    };
}// namespace game