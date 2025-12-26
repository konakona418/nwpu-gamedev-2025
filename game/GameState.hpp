#pragma once

#include "Core/RefCounted.hpp"
#include <algorithm>

namespace game {
    struct GameManager;

    struct GameState : public moe::AtomicRefCounted<GameState> {
    public:
        friend GameManager;

        virtual ~GameState() = default;

        virtual const moe::StringView getName() const = 0;

        virtual void onEnter(GameManager& ctx) {};
        virtual void onUpdate(GameManager& ctx, float deltaTimeSecs) {};
        virtual void onPhysicsUpdate(GameManager& ctx, float deltaTimeSecs) {};
        virtual void onExit(GameManager& ctx) {};

        virtual void onStateChanged(GameManager& ctx, bool isTopmostState) {};

        void _onEnter(GameManager& ctx) {
            onEnter(ctx);
            for (auto& child: m_childStates) {
                child->_onEnter(ctx);
            }
        }

        void _onUpdate(GameManager& ctx, float deltaTimeSecs) {
            onUpdate(ctx, deltaTimeSecs);
            for (auto& child: m_childStates) {
                child->_onUpdate(ctx, deltaTimeSecs);
            }
            processPendingChildStateChanges(ctx);
        }

        void _onPhysicsUpdate(GameManager& ctx, float deltaTimeSecs) {
            onPhysicsUpdate(ctx, deltaTimeSecs);
            for (auto& child: m_childStates) {
                child->_onPhysicsUpdate(ctx, deltaTimeSecs);
            }
        }

        void _onExit(GameManager& ctx) {
            for (auto& child: m_childStates) {
                child->_onExit(ctx);
            }
            onExit(ctx);
        }

        void _onStateChanged(GameManager& ctx, bool isTopmostState) {
            onStateChanged(ctx, isTopmostState);
            for (auto& child: m_childStates) {
                child->_onStateChanged(ctx, isTopmostState);
            }
        }

        void addChildState(moe::Ref<GameState> childState) {
            MOE_ASSERT(childState, "Cannot add a null child state");
            m_pendingAddChildStates.push_back(childState);
        }

        void removeChildState(moe::Ref<GameState> childState) {
            MOE_ASSERT(childState, "Cannot remove a null child state");
            m_pendingRemoveChildStates.push_back(childState);
        }

        const moe::Vector<moe::Ref<GameState>>& getChildStates() const {
            return m_childStates;
        }

    protected:
        GameState* m_parentState{nullptr};
        moe::Vector<moe::Ref<GameState>> m_childStates;

        moe::Vector<moe::Ref<GameState>> m_pendingAddChildStates;
        moe::Vector<moe::Ref<GameState>> m_pendingRemoveChildStates;

        void _addChildState(GameManager& ctx, moe::Ref<GameState> childState) {
            if (childState->m_parentState) {
                childState->m_parentState->_removeChildState(ctx, childState);
            }

            childState->m_parentState = this;
            m_childStates.push_back(childState);

            childState->_onEnter(ctx);
        }

        void _removeChildState(GameManager& ctx, moe::Ref<GameState> childState) {
            childState->_onExit(ctx);

            m_childStates.erase(
                    std::remove(
                            m_childStates.begin(),
                            m_childStates.end(),
                            childState),
                    m_childStates.end());
            childState->m_parentState = nullptr;
        }

        void processPendingChildStateChanges(GameManager& ctx) {
            if (m_pendingAddChildStates.empty() && m_pendingRemoveChildStates.empty()) {
                return;
            }

            auto pendingAdd = std::move(m_pendingAddChildStates);
            for (auto& child: pendingAdd) {
                _addChildState(ctx, child);
            }
            MOE_ASSERT(m_pendingAddChildStates.empty(), "Child states should have been moved");

            auto pendingRemove = std::move(m_pendingRemoveChildStates);
            for (auto& child: pendingRemove) {
                _removeChildState(ctx, child);
            }
            MOE_ASSERT(m_pendingRemoveChildStates.empty(), "Child states should have been moved");
        }
    };
}// namespace game