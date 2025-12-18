#include "GameManager.hpp"
#include "App.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    moe::VulkanEngine& GameManager::renderer() {
        return *m_app->m_graphicsEngine;
    }

    moe::PhysicsEngine& GameManager::physics() {
        return *m_app->m_physicsEngine;
    }

    moe::AudioEngineInterface& GameManager::audio() {
        return *m_app->m_audioEngine;
    }

    game::InputLock GameManager::input() {
        return input(NO_LOCK_TOKEN);
    }

    game::InputLock GameManager::input(InputLockToken lockToken) {
        if (m_inputLockToken != NO_LOCK_TOKEN && m_inputLockToken != lockToken) {
            return game::InputLock(nullptr);
        }

        m_inputLockToken = lockToken;
        return game::InputLock(m_app->m_input.get());
    }

    void GameManager::unlockInput(InputLockToken lockToken) {
        if (m_inputLockToken == lockToken) {
            m_inputLockToken = NO_LOCK_TOKEN;
        } else {
            moe::Logger::error("Attempt to unlock input with invalid lock token");
        }
    }

    void GameManager::pushState(moe::Ref<GameState> state) {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        m_pendingActions.push_back({ActionType::Push, state});
    }

    void GameManager::popState() {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        m_pendingActions.push_back({ActionType::Pop, moe::Ref<GameState>(nullptr)});
    }

    bool GameManager::processPendingActions() {
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

    void GameManager::update(float deltaTimeSecs) {
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

    void GameManager::physicsUpdate(float deltaTimeSecs) {
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
}// namespace game