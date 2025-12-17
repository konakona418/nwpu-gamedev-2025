#pragma once

#include "Audio/AudioEngine.hpp"
#include "Core/FileReader.hpp"
#include "Core/Task/Scheduler.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

#include "GameManager.hpp"

namespace game {
    struct App;

    struct Input {
    public:
        friend struct game::App;

        Input(App* app);

        void update();

        void addKeyMapping(const moe::StringView keyName, int keyCode);
        void removeKeyMapping(const moe::StringView keyName);

        bool isKeyPressed(const moe::StringView keyName) const;
        moe::Pair<float, float> getMouseDelta() const { return m_mouseDelta; }

        void setMouseState(bool isFree);

    private:
        struct KeyMap : moe::AtomicRefCounted<KeyMap> {
            int keyCode;
            bool pressed;
        };

        struct MouseButtonState {
            bool pressedLMB;
            bool pressedRMB;
            bool pressedMMB;
        };

        moe::UnorderedMap<int, moe::Ref<KeyMap>> m_keyMap;
        moe::UnorderedMap<moe::String, moe::Ref<KeyMap>> m_keyNameMap;

        moe::Pair<float, float> m_mouseDelta;

        moe::InputBus* m_inputBus;
        App* m_app;

        moe::Vector<moe::WindowEvent> m_fallThroughEvents;
    };


    struct App {
    public:
        friend struct game::GameManager;
        friend struct game::Input;

        App() {};

        void init();
        void shutdown();
        void run();

    private:
        moe::UniquePtr<moe::VulkanEngine> m_graphicsEngine;
        moe::PhysicsEngine* m_physicsEngine;
        moe::UniquePtr<moe::AudioEngineInterface> m_audioEngine;

        moe::UniquePtr<game::GameManager> m_gameManager;

        moe::UniquePtr<Input> m_input;
    };
}// namespace game