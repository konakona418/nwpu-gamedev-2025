#pragma once

#include "GameState.hpp"

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    struct App;
    struct Input;

    namespace State {
        struct DebugToolState;
    }

    struct GameManager {
    public:
        friend State::DebugToolState;

        GameManager(App* app) : m_app(app) {}

        void pushState(moe::Ref<GameState> state);
        void popState();
        bool processPendingActions();
        void update(float deltaTimeSecs);
        void physicsUpdate(float deltaTimeSecs);

        moe::VulkanEngine& renderer();
        moe::PhysicsEngine& physics();
        moe::AudioEngineInterface& audio();

        game::Input& input();

        void addDebugDrawFunction(
                const moe::StringView name,
                moe::Function<void()> drawFunction);

        void setDebugDrawFunctionActive(const moe::StringView name, bool isActive);

        void removeDebugDrawFunction(const moe::StringView name);

        template<typename F>
        void addDebugDrawFunction(
                const moe::StringView name,
                F&& drawFunction) {
            addDebugDrawFunction(
                    name,
                    moe::Function<void()>(std::forward<F>(drawFunction)));
        }

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

        struct DebugDrawFunction {
            moe::Function<void()> drawFunction;
            bool isActive{false};
        };

        moe::UnorderedMap<moe::String, DebugDrawFunction> m_debugDrawFunctions;
    };
}// namespace game