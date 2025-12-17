#include "PlaygroundState.hpp"
#include "GameManager.hpp"

#include "Math/Transform.hpp"
#include "Physics/GltfColliderFactory.hpp"
#include "Physics/PhysicsEngine.hpp"
#include "Physics/TypeUtils.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"


namespace game::State {
    void PlaygroundState::onEnter(GameManager& ctx) {
        auto playgroundAsset = moe::asset("assets/models/playground.glb");

        m_playgroundRenderable = ctx.renderer().getResourceLoader().load(moe::Loader::Gltf, playgroundAsset);
        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<PlaygroundState>(), playgroundAsset](moe::PhysicsEngine& physics) mutable {
                    moe::Logger::debug("Creating playground collider");

                    auto collider = moe::Physics::GltfColliderFactory::shapeFromGltf(playgroundAsset);
                    JPH::BodyCreationSettings settings = JPH::BodyCreationSettings(
                            collider,
                            JPH::RVec3(0.0f, 0.0f, 0.0f),
                            JPH::Quat::sIdentity(),
                            JPH::EMotionType::Static,
                            moe::Physics::Details::Layers::NON_MOVING);

                    auto& bodyInterface = physics.getPhysicsSystem().GetBodyInterface();
                    state->m_playgroundBody.set(bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate));
                });
    }

    void PlaygroundState::onExit(GameManager& ctx) {
        ctx.physics().dispatchOnPhysicsThread([state = this->asRef<PlaygroundState>()](moe::PhysicsEngine& physics) {
            moe::Logger::debug("Removing playground collider");

            auto& bodyInterface = physics.getPhysicsSystem().GetBodyInterface();
            bodyInterface.RemoveBody(state->m_playgroundBody.get().value());
            bodyInterface.DestroyBody(state->m_playgroundBody.get().value());
        });
    }

    void PlaygroundState::onUpdate(GameManager& ctx, float) {
        auto& physicsSwap = ctx.physics().getCurrentRead();
        if (auto body = m_playgroundBody.get()) {
            auto pos = physicsSwap.getSnapshot(body.value())->position;
            ctx.renderer()
                    .getBus<moe::VulkanRenderObjectBus>()
                    .submitRender(m_playgroundRenderable, moe::Transform{}.setPosition(moe::Physics::fromJoltType<glm::vec3>(pos)));
        } else {
            return;
        }
    }
}// namespace game::State