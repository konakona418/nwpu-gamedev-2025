#include "State/LocalPlayerState.hpp"

#include "State/PauseUIState.hpp"

#include "App.hpp"
#include "GameManager.hpp"
#include "InputUtil.hpp"

#include "Physics/TypeUtils.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"


namespace game::State {

#define PLAYER_KEY_MAPPING_XXX() \
    X(forward, GLFW_KEY_W);      \
    X(backward, GLFW_KEY_S);     \
    X(left, GLFW_KEY_A);         \
    X(right, GLFW_KEY_D);        \
    X(up, GLFW_KEY_SPACE);       \
    X(down, GLFW_KEY_LEFT_SHIFT);

    void LocalPlayerState::onEnter(GameManager& ctx) {
        auto input = ctx.input();
        if (!input.isValid()) {
            moe::Logger::error("LocalPlayerState::onEnter: input is locked by another state");
            return;
        }

#define X(name, key) input->addKeyMapping(#name, key);
        PLAYER_KEY_MAPPING_XXX()
#undef X

        input->addKeyEventMapping("escape_player", GLFW_KEY_ESCAPE);

        input->setMouseState(false);// disable mouse cursor

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
        auto input = ctx.input();
        if (!input.isValid()) {
            moe::Logger::error("LocalPlayerState::onExit: input is locked by another state");
            return;
        }

        input->setMouseState(true);// enable mouse cursor

#define X(name, key) input->removeKeyMapping(#name);
        PLAYER_KEY_MAPPING_XXX()
#undef X

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<LocalPlayerState>()](moe::PhysicsEngine& physics) mutable {
                    state->m_character.set(nullptr);// release character
                });
    }

    void LocalPlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        auto input = ctx.input();

        auto& cam = ctx.renderer().getDefaultCamera();

        glm::vec3 dir{0.0f};
        float yawDelta = 0.0f;
        float pitchDelta = 0.0f;

        if (input.isValid()) {
            // only process input if we have the input lock

            MovementHelper movementHelper;
#define X(name, key) movementHelper.name = input->isKeyPressed(#name)
            PLAYER_KEY_MAPPING_XXX()
#undef X

            auto front = cam.getFront();
            front.y = 0;// proj to xOz
            front = glm::normalize(front);
            dir = movementHelper.realMovementVec(front, cam.getRight(), cam.getUp());

            auto [mouseX, mouseY] = input->getMouseDelta();
            yawDelta = mouseX * PLAYER_ROTATION_SPEED;
            pitchDelta = mouseY * PLAYER_ROTATION_SPEED;

            // if escape is just released, show menu
            if (input->isKeyJustReleased("escape_player")) {
                ctx.pushState(moe::Ref(new State::PauseUIState()));
            }
        }

        auto pos = m_realPosition.get();
        cam.setPosition(pos);

        cam.setYaw(cam.getYaw() + yawDelta);
        cam.setPitch(cam.getPitch() - pitchDelta);

        m_movingDirection.publish(std::move(dir));
    }

    void LocalPlayerState::onPhysicsUpdate(GameManager& ctx, float deltaTime) {
        auto characterOpt = m_character.get();
        if (!characterOpt) {
            return;
        }

        auto character = characterOpt.value();
        if (!character) {
            return;
        }

        auto& physicsSystem = ctx.physics().getPhysicsSystem();

        auto dir = m_movingDirection.get();
        auto velocity = dir * PLAYER_SPEED;

        auto lastVel = character->GetLinearVelocity();

        auto velXoZ = moe::Physics::toJoltType<JPH::Vec3>(glm::vec3(velocity.x, 0, velocity.z));
        auto velY = velocity.y;

        auto groundState = character->GetGroundState();
        bool onGround = groundState != JPH::CharacterVirtual::EGroundState::InAir &&     // flying
                        groundState != JPH::CharacterVirtual::EGroundState::NotSupported;// about to fall

        auto gravity = -character->GetUp() * physicsSystem.GetGravity().Length();
        if (onGround) {
            // on ground, use input Y velocity
            velY = velocity.y;
        } else {
            // in air, preserve Y velocity
            velY = lastVel.GetY() + gravity.GetY() * deltaTime;
        }

        JPH::Vec3 finalVel(velXoZ.GetX(), velY, velXoZ.GetZ());

        // check collision
        JPH::CharacterVirtual::ExtendedUpdateSettings settings;
        // todo: fill in this settings, to allow character to climb up

        character->SetLinearVelocity(finalVel);
        character->ExtendedUpdate(
                deltaTime,
                gravity,
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