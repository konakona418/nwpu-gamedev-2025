#pragma once

#include "GameState.hpp"

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    struct App;
    struct Input;

    using InputLockToken = uint32_t;
    inline constexpr InputLockToken NO_LOCK_TOKEN = std::numeric_limits<InputLockToken>::max();

    struct InputLock {
    public:
        explicit InputLock(Input* input) : m_input(input) {};

        Input* get() const { return m_input; }
        Input* operator->() const { return m_input; }
        Input& operator*() const { return *m_input; }

        bool isValid() const { return m_input != nullptr; }
        operator bool() const { return isValid(); }

    private:
        Input* m_input;
    };

    struct GameManager {
    public:
        GameManager(App* app) : m_app(app) {}

        void pushState(moe::Ref<GameState> state);
        void popState();
        bool processPendingActions();
        void update(float deltaTimeSecs);
        void physicsUpdate(float deltaTimeSecs);

        moe::VulkanEngine& renderer();
        moe::PhysicsEngine& physics();
        moe::AudioEngineInterface& audio();

        game::InputLock input();
        game::InputLock input(InputLockToken lockToken);
        void unlockInput(InputLockToken lockToken);

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
        InputLockToken m_inputLockToken{NO_LOCK_TOKEN};
    };
}// namespace game