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

    game::Input& GameManager::input() {
        return *m_app->m_input;
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

    void GameManager::addDebugDrawFunction(
            const moe::StringView name,
            moe::Function<void()> drawFunction) {
        m_debugDrawFunctions[name.data()] = DebugDrawFunction{drawFunction, false};
    }

    void GameManager::setDebugDrawFunctionActive(const moe::StringView name, bool isActive) {
        auto it = m_debugDrawFunctions.find(name.data());
        if (it != m_debugDrawFunctions.end()) {
            it->second.isActive = isActive;
        }
    }

    void GameManager::removeDebugDrawFunction(const moe::StringView name) {
        auto it = m_debugDrawFunctions.find(name.data());
        if (it != m_debugDrawFunctions.end()) {
            m_debugDrawFunctions.erase(it);
        }
    }
}// namespace game