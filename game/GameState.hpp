#pragma once

#include "Core/RefCounted.hpp"

#include <algorithm>

namespace game {
    struct GameManager;

    struct GameState : public moe::AtomicRefCounted<GameState> {
    public:
        friend GameManager;

        virtual const moe::StringView getName() const = 0;

        virtual void onEnter(GameManager& ctx) {};
        virtual void onUpdate(GameManager& ctx, float deltaTimeSecs) {};
        virtual void onPhysicsUpdate(GameManager& ctx, float deltaTimeSecs) {};
        virtual void onExit(GameManager& ctx) {};

        virtual void onStateChanged(GameManager& ctx, bool isTopmostState) {};

        void _onEnter(GameManager& ctx);

        void _onUpdate(GameManager& ctx, float deltaTimeSecs);

        void _onPhysicsUpdate(GameManager& ctx, float deltaTimeSecs);

        void _onExit(GameManager& ctx);

        void _onStateChanged(GameManager& ctx, bool isTopmostState);

        void addChildState(moe::Ref<GameState> childState);

        void removeChildState(moe::Ref<GameState> childState);

        const moe::Vector<moe::Ref<GameState>>& getChildStates() const {
            return m_childStates;
        }

        virtual ~GameState();

    protected:
        struct SnapShot : public moe::AtomicRefCounted<SnapShot> {
            moe::Vector<moe::Ref<GameState>> childStates;
        };

        GameState* m_parentState{nullptr};
        moe::Vector<moe::Ref<GameState>> m_childStates;

        std::mutex m_childStateMutex;
        std::atomic<SnapShot*> m_childStateSnapShot{nullptr};

        moe::Vector<moe::Ref<GameState>> m_pendingAddChildStates;
        moe::Vector<moe::Ref<GameState>> m_pendingRemoveChildStates;

        void _addChildState(GameManager& ctx, moe::Ref<GameState> childState);

        void _removeChildState(GameManager& ctx, moe::Ref<GameState> childState);

        void processPendingChildStateChanges(GameManager& ctx);

        moe::Ref<SnapShot> getChildStatesSnapshot();
    };
}// namespace game