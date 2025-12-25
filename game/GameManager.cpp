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

    game::NetworkAdaptor& GameManager::network() {
        return *m_app->m_networkAdaptor;
    }

    game::Input& GameManager::input() {
        return *m_app->m_input;
    }

    void GameManager::pushState(moe::Ref<GameState> state) {
        MOE_ASSERT(state, "Cannot push a null state");
        std::lock_guard<std::mutex> lock(m_actionMutex);
        m_pendingActions.push_back({ActionType::Push, state});
    }

    void GameManager::popState() {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        m_pendingActions.push_back({ActionType::Pop, moe::Ref<GameState>::null()});
    }

    void GameManager::queueFree(moe::Ref<GameState> state) {
        if (!state) {
            return;
        }

        if (!state->m_parentState) {
            if (m_gameStateStack.empty() || m_gameStateStack.back() != state) {
                moe::Logger::warn(
                        "GameManager::queueFree: Attempted to free a root state that is not on the stack top: {}",
                        state->getName());
                return;
            }
            // is top-level state, just pop it
            popState();
        } else {
            // remove from parent state, thus will leave the scene tree and get freed
            state->m_parentState->removeChildState(state);
        }

        moe::Logger::debug("GameManager::queueFree: Queued state '{}' for freeing", state->getName());
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
                case ActionType::Push: {
                    m_gameStateStack.push_back(action.state);
                    action.state->_onEnter(*this);
                    break;
                }
                case ActionType::Pop: {
                    auto& stateToPop = m_gameStateStack.back();
                    stateToPop->_onExit(*this);// call onExit before removing
                    m_gameStateStack.pop_back();
                    break;
                }
            }
        }
        m_pendingActions.clear();

        return true;
    }

    void GameManager::update(float deltaTimeSecs) {
        bool diff = processPendingActions();

        if (m_gameStateStack.empty()) return;

        // non-persistent
        if (!m_gameStateStack.empty()) {
            // only update the top state
            m_gameStateStack.back()->_onUpdate(*this, deltaTimeSecs);
        }

        // persistent
        for (auto& state: m_persistentGameStateStack) {
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

        if (!stackCopy.empty()) {
            // non-persistent
            stackCopy.back()->_onPhysicsUpdate(*this, deltaTimeSecs);
        }

        // persistent
        // as persistent states are not supposed to be changed during game loop,
        // we can directly use m_persistentGameStateStack here
        for (auto& state: m_persistentGameStateStack) {
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

    void GameManager::addPersistGameState(moe::Ref<GameState> state) {
        m_persistentGameStateStack.push_back(state);
        state->_onEnter(*this);// call onEnter when adding persistent state
    }
}// namespace game