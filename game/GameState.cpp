#include "GameState.hpp"

namespace game {
    GameState::~GameState() {
        SnapShot* snap = m_childStateSnapShot.load(std::memory_order_acquire);
        if (snap) {
            snap->release();
        }
    }

    void GameState::_onEnter(GameManager& ctx) {
        onEnter(ctx);

        // create snapshot for the first time
        if (!m_childStateSnapShot.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(m_childStateMutex);
            SnapShot* newSnap = new SnapShot();
            newSnap->childStates = m_childStates;
            newSnap->retain();
            m_childStateSnapShot.store(newSnap, std::memory_order_release);
        }

        // ! watch out for this direct usage of m_childStates
        for (auto& child: m_childStates) {
            child->_onEnter(ctx);
        }
    }

    void GameState::_onUpdate(GameManager& ctx, float deltaTimeSecs) {
        onUpdate(ctx, deltaTimeSecs);
        auto snapshot = getChildStatesSnapshot();
        if (!snapshot) {
            return;
        }

        for (auto& child: snapshot->childStates) {
            child->_onUpdate(ctx, deltaTimeSecs);
        }
        processPendingChildStateChanges(ctx);
    }

    void GameState::_onPhysicsUpdate(GameManager& ctx, float deltaTimeSecs) {
        onPhysicsUpdate(ctx, deltaTimeSecs);
        auto snapshot = getChildStatesSnapshot();
        if (!snapshot) {
            return;
        }

        for (auto& child: snapshot->childStates) {
            child->_onPhysicsUpdate(ctx, deltaTimeSecs);
        }
    }

    void GameState::_onExit(GameManager& ctx) {
        // ! watch out for this
        for (auto& child: m_childStates) {
            child->_onExit(ctx);
        }
        onExit(ctx);
    }

    void GameState::_onStateChanged(GameManager& ctx, bool isTopmostState) {
        onStateChanged(ctx, isTopmostState);
        for (auto& child: m_childStates) {
            child->_onStateChanged(ctx, isTopmostState);
        }
    }

    void GameState::addChildState(moe::Ref<GameState> childState) {
        MOE_ASSERT(childState, "Cannot add a null child state");
        m_pendingAddChildStates.push_back(childState);
    }

    void GameState::removeChildState(moe::Ref<GameState> childState) {
        MOE_ASSERT(childState, "Cannot remove a null child state");
        m_pendingRemoveChildStates.push_back(childState);
    }

    void GameState::_addChildState(GameManager& ctx, moe::Ref<GameState> childState) {
        if (childState->m_parentState) {
            childState->m_parentState->_removeChildState(ctx, childState);
        }

        childState->m_parentState = this;
        m_childStates.push_back(childState);
    }

    void GameState::_removeChildState(GameManager& ctx, moe::Ref<GameState> childState) {
        m_childStates.erase(
                std::remove(
                        m_childStates.begin(),
                        m_childStates.end(),
                        childState),
                m_childStates.end());
        childState->m_parentState = nullptr;
    }

    void GameState::processPendingChildStateChanges(GameManager& ctx) {
        if (m_pendingAddChildStates.empty() && m_pendingRemoveChildStates.empty()) {
            return;
        }

        moe::Vector<moe::Ref<GameState>> toEnter;
        moe::Vector<moe::Ref<GameState>> toExit;

        {
            std::lock_guard<std::mutex> lock(m_childStateMutex);

            auto pendingAdd = std::move(m_pendingAddChildStates);
            for (auto& child: pendingAdd) {
                _addChildState(ctx, child);
                toEnter.push_back(child);
            }

            auto pendingRemove = std::move(m_pendingRemoveChildStates);
            for (auto& child: pendingRemove) {
                _removeChildState(ctx, child);
                toExit.push_back(child);
            }

            SnapShot* newSnap = new SnapShot();
            newSnap->childStates = m_childStates;
            newSnap->retain();

            SnapShot* oldSnap = m_childStateSnapShot.exchange(newSnap, std::memory_order_acq_rel);
            if (oldSnap) oldSnap->release();
        }

        for (auto& child: toExit) child->_onExit(ctx);
        for (auto& child: toEnter) child->_onEnter(ctx);
    }

    moe::Ref<GameState::SnapShot> GameState::getChildStatesSnapshot() {
        return moe::Ref<SnapShot>(m_childStateSnapShot.load(std::memory_order_acquire));
    }
}// namespace game