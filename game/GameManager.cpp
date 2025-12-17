#include "GameManager.hpp"
#include "App.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"

namespace game {
    moe::VulkanEngine& GameManager::renderer() {
        return *m_app->m_graphicsEngine;
    }

    moe::PhysicsEngine& GameManager::physics() {
        return *m_app->m_physicsEngine;
    }

    moe::AudioEngineInterface& GameManager::audio() {
        return *m_app->m_audioEngine;
    }

    game::Input& GameManager::input() {
        return *m_app->m_input;
    }
}// namespace game