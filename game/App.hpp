#pragma once

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

#include "GameManager.hpp"
#include "Input.hpp"

namespace game {
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