#pragma once

#include "Audio/AudioEngine.hpp"
#include "Core/FileReader.hpp"
#include "Core/Task/Scheduler.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

#include "GameManager.hpp"

namespace game {
    struct App {
    public:
        friend struct game::GameManager;

        App() = default;

        void init();
        void shutdown();
        void run();

    private:
        moe::UniquePtr<moe::VulkanEngine> m_graphicsEngine;
        moe::PhysicsEngine* m_physicsEngine;
        moe::UniquePtr<moe::AudioEngineInterface> m_audioEngine;

        moe::UniquePtr<game::GameManager> m_gameManager;
    };
}// namespace game