#pragma once

#include "Audio/AudioEngine.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"

#include "GameManager.hpp"
#include "Input.hpp"
#include "NetworkAdaptor.hpp"

namespace game {
    struct App {
    public:
        friend struct game::GameManager;
        friend struct game::Input;

        struct Stats {
        public:
            float fps{0.0f};
            float frameTimeMs{0.0f};
        };

        App() {};

        void init();
        void shutdown();
        void run();

        const Stats& getStats() const { return m_stats; }

    private:
        moe::UniquePtr<moe::VulkanEngine> m_graphicsEngine;
        moe::PhysicsEngine* m_physicsEngine;
        moe::UniquePtr<moe::AudioEngineInterface> m_audioEngine;

        moe::UniquePtr<game::GameManager> m_gameManager;

        moe::UniquePtr<Input> m_input;
        moe::UniquePtr<NetworkAdaptor> m_networkAdaptor;

        Stats m_stats;
    };
}// namespace game