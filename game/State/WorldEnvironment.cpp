#include "State/WorldEnvironment.hpp"

#include "GameManager.hpp"

namespace game::State {
    void WorldEnvironment::onUpdate(GameManager& ctx, float deltaTime) {
        auto& illumination = ctx.renderer().getBus<moe::VulkanIlluminationBus>();
        illumination.setAmbient(glm::vec3{1.0f}, 0.05);
        illumination.setSunlight(glm::vec3{-1.0f, -1.0f, 0.0f}, glm::vec3{1.0f}, 0.2);
    }
}// namespace game::State