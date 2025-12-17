#pragma once

#include "Core/RefCounted.hpp"
#include <algorithm>

namespace game {
    struct GameManager;

    struct GameState : public moe::AtomicRefCounted<GameState> {
    public:
        virtual ~GameState() = default;

        virtual const moe::String& getName() const = 0;

        virtual void onEnter(GameManager& ctx) {};
        virtual void onUpdate(GameManager& ctx, float deltaTimeSecs) {};
        virtual void onPhysicsUpdate(GameManager& ctx, float deltaTimeSecs) {};
        virtual void onExit(GameManager& ctx) {};

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

        void addChildState(moe::Ref<GameState> childState) {
            if (childState->m_parentState) {
                childState->m_parentState->removeChildState(childState);
            }

            childState->m_parentState = moe::Ref<GameState>(this);
            m_childStates.push_back(childState);
        }

        void removeChildState(moe::Ref<GameState> childState) {
            m_childStates.erase(
                    std::remove(
                            m_childStates.begin(),
                            m_childStates.end(),
                            childState),
                    m_childStates.end());
            childState->m_parentState = moe::Ref<GameState>(nullptr);
        }

    protected:
        moe::Ref<GameState> m_parentState{nullptr};
        moe::Vector<moe::Ref<GameState>> m_childStates;
    };
}// namespace game