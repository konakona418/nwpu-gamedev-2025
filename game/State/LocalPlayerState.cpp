#include "State/LocalPlayerState.hpp"

#include "App.hpp"
#include "GameManager.hpp"
#include "InputUtil.hpp"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Physics/TypeUtils.hpp"

namespace game::State {

#define PLAYER_KEY_MAPPING_XXX() \
    X(forward, GLFW_KEY_W);      \
    X(backward, GLFW_KEY_S);     \
    X(left, GLFW_KEY_A);         \
    X(right, GLFW_KEY_D);        \
    X(up, GLFW_KEY_SPACE);       \
    X(down, GLFW_KEY_LEFT_SHIFT);

    void LocalPlayerState::onEnter(GameManager& ctx) {
        auto& input = ctx.input();

#define X(name, key) input.addKeyMapping(#name, key);
        PLAYER_KEY_MAPPING_XXX()
#undef X

        input.setMouseState(false);// disable mouse cursor

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<LocalPlayerState>()](moe::PhysicsEngine& physics) mutable {
                    auto& bodyInterface = physics.getPhysicsSystem().GetBodyInterface();

                    auto capsuleShape =
                            JPH::CapsuleShapeSettings(
                                    PLAYER_HALF_HEIGHT,
                                    PLAYER_RADIUS)
                                    .Create()
                                    .Get();
                    auto characterSettings = JPH::CharacterVirtualSettings();
                    characterSettings.mShape = capsuleShape;

                    auto character =
                            new JPH::CharacterVirtual(
                                    &characterSettings,
                                    JPH::RVec3Arg(0, 5.0, 0),
                                    JPH::QuatArg::sIdentity(),
                                    &physics.getPhysicsSystem());

                    state->m_character.set(character);
                });
    }

    void LocalPlayerState::onExit(GameManager& ctx) {
        auto& input = ctx.input();

#define X(name, key) input.removeKeyMapping(#name);
        PLAYER_KEY_MAPPING_XXX()
#undef X

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<LocalPlayerState>()](moe::PhysicsEngine& physics) mutable {
                    state->m_character.set(nullptr);// release character
                });
    }

    void LocalPlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& input = ctx.input();

        MovementHelper movementHelper;
#define X(name, key) movementHelper.name = input.isKeyPressed(#name)
        PLAYER_KEY_MAPPING_XXX()
#undef X

        auto& cam = ctx.renderer().getDefaultCamera();

        auto front = cam.getFront();
        front.y = 0;// proj to xOz
        auto dir = movementHelper.realMovementVec(front, cam.getRight(), cam.getUp());

        m_movingDirection.publish(std::move(dir));

        auto [mouseX, mouseY] = input.getMouseDelta();
        float yawDelta = mouseX * PLAYER_ROTATION_SPEED;
        float pitchDelta = mouseY * PLAYER_ROTATION_SPEED;

        auto pos = m_realPosition.get();
        cam.setPosition(pos);

        cam.setYaw(cam.getYaw() + yawDelta);
        cam.setPitch(cam.getPitch() - pitchDelta);
    }

    void LocalPlayerState::onPhysicsUpdate(GameManager& ctx, float deltaTime) {
        auto dir = m_movingDirection.get();
        auto velocity = dir * PLAYER_SPEED;

        auto characterOpt = m_character.get();
        if (!characterOpt) {
            return;
        }

        auto character = characterOpt.value();
        if (!character) {
            return;
        }

        // check collision
        auto& physicsSystem = ctx.physics().getPhysicsSystem();
        JPH::CharacterVirtual::ExtendedUpdateSettings settings;
        // todo: fill in this settings, to allow character to climb up

        character->SetLinearVelocity(moe::Physics::toJoltType<JPH::Vec3>(velocity));
        character->ExtendedUpdate(
                deltaTime,
                -character->GetUp() * physicsSystem.GetGravity().Length(),
                settings,
                physicsSystem.GetDefaultBroadPhaseLayerFilter(
                        moe::Physics::Details::Layers::MOVING),
                physicsSystem.GetDefaultLayerFilter(
                        moe::Physics::Details::Layers::MOVING),
                {}, {},
                *ctx.physics().getTempAllocator());

        m_realPosition.publish(moe::Physics::fromJoltType<glm::vec3>(character->GetPosition()));
    }

#undef PLAYER_KEY_MAPPING_XXX
}// namespace game::State