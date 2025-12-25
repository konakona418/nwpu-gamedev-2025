#pragma once

#include "GameState.hpp"

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    struct App;
    struct Input;
    struct NetworkAdaptor;

    namespace State {
        struct DebugToolState;
    }

    struct GameManager {
    public:
        friend State::DebugToolState;

        GameManager(App* app) : m_app(app) {}

        void pushState(moe::Ref<GameState> state);
        void popState();

        void queueFree(moe::Ref<GameState> state);

        bool processPendingActions();
        void update(float deltaTimeSecs);
        void physicsUpdate(float deltaTimeSecs);

        moe::VulkanEngine& renderer();
        moe::PhysicsEngine& physics();
        moe::AudioEngineInterface& audio();

        game::NetworkAdaptor& network();

        game::Input& input();

        App& app() { return *m_app; }

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

        void addPersistGameState(moe::Ref<GameState> state);

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

        moe::Vector<moe::Ref<game::GameState>> m_gameStateStack;

        moe::Vector<moe::Ref<game::GameState>> m_gameStateStackCopy;
        std::mutex m_gameStateStackCopyMutex;

        // this is for states that should survive the whole lifecycle of the program
        // e.g. debug tools, world environments, etc.
        // ! this should not be modified in game loop
        // currently the onExit of persistent states is not called
        moe::Vector<moe::Ref<game::GameState>> m_persistentGameStateStack;

        App* m_app = nullptr;

        struct DebugDrawFunction {
            moe::Function<void()> drawFunction;
            bool isActive{false};
        };

        moe::UnorderedMap<moe::String, DebugDrawFunction> m_debugDrawFunctions;

        moe::Vector<moe::Ref<GameState>> getCurrentStateStack() const {
            return m_gameStateStack;
        }

        moe::Vector<moe::Ref<GameState>> getPersistentStateStack() const {
            return m_persistentGameStateStack;
        }
    };
}// namespace game