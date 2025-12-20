#pragma once

#include "Audio/AudioEngine.hpp"
#include "Core/FileReader.hpp"
#include "Core/Task/Scheduler.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

#include "GameManager.hpp"

namespace game {
    struct App;

    struct Input : moe::Meta::NonCopyable<Input> {
    public:
        friend struct game::App;

        struct MouseButtonState {
            bool pressedLMB;
            bool pressedRMB;
            bool pressedMMB;
        };

        Input(App* app);

        void update();

        void addKeyMapping(const moe::StringView keyName, int keyCode);
        void removeKeyMapping(const moe::StringView keyName);

        void addKeyEventMapping(const moe::StringView keyName, int keyCode);
        void removeKeyEventMapping(const moe::StringView keyName);

        bool isKeyPressed(const moe::StringView keyName) const;
        bool isKeyJustPressed(const moe::StringView keyName) const;
        bool isKeyJustReleased(const moe::StringView keyName) const;
        moe::Pair<float, float> getMouseDelta() const { return m_mouseDelta; }
        moe::Pair<float, float> getMousePosition() const;
        MouseButtonState getMouseButtonState(int button) const;

        void setMouseState(bool isFree);
        bool isMouseFree() const;

    private:
        struct KeyMap : moe::AtomicRefCounted<KeyMap> {
            int keyCode;
            bool pressed;
        };

        struct KeyEventMap : moe::AtomicRefCounted<KeyEventMap> {
            int keyCode;
            bool justPressed;
            bool justReleased;
        };

        moe::UnorderedMap<int, moe::Ref<KeyMap>> m_keyMap;
        moe::UnorderedMap<moe::String, moe::Ref<KeyMap>> m_keyNameMap;

        moe::UnorderedMap<int, moe::Ref<KeyEventMap>> m_keyEventMap;
        moe::UnorderedMap<moe::String, moe::Ref<KeyEventMap>> m_keyEventNameMap;

        moe::Pair<float, float> m_mouseDelta;

        moe::InputBus* m_inputBus;
        App* m_app;

        moe::Vector<moe::WindowEvent> m_fallThroughEvents;

        void dispatchKeyDown(int keyCode);
        void dispatchKeyUp(int keyCode);
        void resetKeyEvents();
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