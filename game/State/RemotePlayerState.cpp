#include "State/RemotePlayerState.hpp"

#include "Physics/TypeUtils.hpp"

#include "State/PlayerSharedConfig.hpp"

#include "NetworkDispatcher.hpp"
#include "Registry.hpp"


#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

#include "im3d.h"
#include "imgui.h"

namespace game::State {
    void RemotePlayerState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering RemotePlayerState, id={}", m_playerTempId);
        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<RemotePlayerState>()](moe::PhysicsEngine& physics) mutable {
                    auto& bodyInterface = physics.getPhysicsSystem().GetBodyInterface();

                    auto capsuleShape =
                            JPH::CapsuleShapeSettings(
                                    PLAYER_HALF_HEIGHT,
                                    PLAYER_RADIUS)
                                    .Create()
                                    .Get();
                    auto characterSettings = JPH::CharacterVirtualSettings();
                    characterSettings.mShape = capsuleShape;
                    // to prevent wall climbing
                    characterSettings.mSupportingVolume =
                            JPH::Plane(
                                    JPH::Vec3::sAxisY(),
                                    PLAYER_SUPPORTING_VOLUME_CONSTANT.get());
                    characterSettings.mMaxSlopeAngle =
                            JPH::DegreesToRadians(PLAYER_MAX_SLOPE_ANGLE_DEGREES.get());

                    auto character =
                            new JPH::CharacterVirtual(
                                    &characterSettings,
                                    JPH::RVec3Arg(0, 5.0, 0),
                                    JPH::QuatArg::sIdentity(),
                                    &physics.getPhysicsSystem());

                    state->m_character.set(character);
                });
    }

    void RemotePlayerState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting RemotePlayerState, id={}", m_playerTempId);

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<RemotePlayerState>()](moe::PhysicsEngine& physics) mutable {
                    state->m_character.set(nullptr);// release character
                });
    }

    void RemotePlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer();

        auto pos = m_realPosition.get();
        auto heading = m_realHeading.get();

        renderer.addIm3dDrawCommand([state = this->asRef<RemotePlayerState>(), pos, heading]() {
            auto cameraOffset = PLAYER_CAMERA_OFFSET_Y;

            Im3d::PushDrawState();
            Im3d::SetColor(Im3d::Color{0.0f, 0.0f, 1.0f, 1.0f});
            Im3d::SetSize(5.0f);
            Im3d::DrawCapsule(
                    Im3d::Vec3{pos.x, pos.y - PLAYER_HALF_HEIGHT, pos.z},
                    Im3d::Vec3{pos.x, pos.y + PLAYER_HALF_HEIGHT, pos.z},
                    PLAYER_RADIUS);
            Im3d::PopDrawState();

            Im3d::PushDrawState();
            Im3d::SetColor(Im3d::Color{1.0f, 0.0f, 0.0f, 1.0f});
            Im3d::SetSize(5.0f);
            Im3d::DrawArrow(
                    Im3d::Vec3{pos.x, pos.y + cameraOffset, pos.z},
                    Im3d::Vec3{
                            pos.x + heading.x,
                            pos.y + cameraOffset,
                            pos.z + heading.z,
                    });
            Im3d::PopDrawState();
        });
    }

    void RemotePlayerState::onPhysicsUpdate(GameManager& ctx, float deltaTime) {
        auto gamePlaySharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!gamePlaySharedData) {
            return;
        }

        auto networkDispatcher = gamePlaySharedData->networkDispatcher;
        if (!networkDispatcher) {
            return;
        }

        while (auto update = networkDispatcher->getPlayerUpdate(m_playerTempId)) {
            if (!update.has_value()) {
                break;
            }

            RemotePlayerMotionInterpolationData motionData;
            motionData.position = update->position;
            motionData.velocity = update->velocity;
            motionData.heading = update->heading;

            m_motionInterpolationBuffer->pushBack(motionData, ctx.physics().getCurrentTickIndex());
        }

        auto characterOpt = m_character.get();
        if (!characterOpt) {
            return;
        }

        auto character = characterOpt.value();
        if (!character) {
            return;
        }

        auto interpData = m_motionInterpolationBuffer->interpolate(deltaTime);
        character->SetPosition(moe::Physics::toJoltType<JPH::Vec3>(interpData.position));

        m_realPosition.publish(interpData.position);
        m_realHeading.publish(interpData.heading);
    }

}// namespace game::State