#include "State/RemotePlayerState.hpp"

#include "Physics/TypeUtils.hpp"

#include "State/PlayerSharedConfig.hpp"

#include "NetworkDispatcher.hpp"
#include "Registry.hpp"


#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

#include "im3d.h"
#include "imgui.h"

namespace game::State {
    static moe::UnorderedMap<moe::String, moe::AnimationId> getAnimationsFromRenderable(GameManager& ctx, moe::RenderableId renderableId) {
        auto scene = ctx.renderer().m_caches.objectCache.get(renderableId).value();
        auto* animatableRenderable = scene->checkedAs<moe::VulkanSkeletalAnimation>(moe::VulkanRenderableFeature::HasSkeletalAnimation).value();
        auto& animations = animatableRenderable->getAnimations();

        return animations;
    }

    void RemotePlayerState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering RemotePlayerState, id={}", m_playerTempId);
        m_terroristModel = m_terroristModelLoader.generate().value_or(moe::NULL_RENDERABLE_ID);
        if (m_terroristModel == moe::NULL_RENDERABLE_ID) {
            moe::Logger::error("Failed to load terrorist model");
            return;
        }
        m_terroristAnimationIds = getAnimationsFromRenderable(ctx, m_terroristModel);

        initAnimationFSM();

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<RemotePlayerState>()](moe::PhysicsEngine& physics) mutable {
                    auto& bodyInterface = physics.getPhysicsSystem().GetBodyInterface();

                    auto capsuleShape =
                            JPH::CapsuleShapeSettings(
                                    Config::PLAYER_HALF_HEIGHT,
                                    Config::PLAYER_RADIUS)
                                    .Create()
                                    .Get();
                    auto characterSettings = JPH::CharacterVirtualSettings();
                    characterSettings.mShape = capsuleShape;
                    // to prevent wall climbing
                    characterSettings.mSupportingVolume =
                            JPH::Plane(
                                    JPH::Vec3::sAxisY(),
                                    Config::PLAYER_SUPPORTING_VOLUME_CONSTANT.get());
                    characterSettings.mMaxSlopeAngle =
                            JPH::DegreesToRadians(Config::PLAYER_MAX_SLOPE_ANGLE_DEGREES.get());

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

    void RemotePlayerState::updateAnimationFSM(GameManager& ctx, float deltaTime) {
        auto pos = m_realPosition.get();
        auto heading = m_realHeading.get();
        auto velocity = m_realVelocity.get();
        glm::vec3 flatVelocity = glm::vec3(velocity.x, 0.0f, velocity.z);

        bool isMovingForward = glm::length(flatVelocity) > 0.1f &&
                               glm::dot(glm::normalize(flatVelocity), glm::normalize(heading)) > 0.1f;
        bool isMovingBackward = glm::length(flatVelocity) > 0.1f &&
                                glm::dot(glm::normalize(flatVelocity), glm::normalize(heading)) < -0.1f;

        m_animationFSM.setStateArg("move_forward", isMovingForward);
        m_animationFSM.setStateArg("move_backward", isMovingBackward);
        // todo: set hit and jump args later

        m_animationFSM.update(deltaTime);
        auto animationSample = m_animationFSM.sampleCurrentAnimation();

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        auto computeHandle = renderctx.submitComputeSkin(
                m_terroristModel,
                m_terroristAnimationIds[animationSample.animation.name],
                animationSample.timeInAnimation);

        auto rotation = std::atan2(heading.x, heading.z);
        auto realPosition = m_realPosition.get();
        realPosition.y -= (Config::PLAYER_HALF_HEIGHT + Config::PLAYER_RADIUS);// offset to feet position

        renderctx.submitRender(
                m_terroristModel,
                moe::Transform{}
                        .setPosition(realPosition)
                        .setRotation(glm::vec3(0.0f, rotation, 0.0f)),
                computeHandle);
    }

    void RemotePlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer();

        auto pos = m_realPosition.get();
        auto heading = m_realHeading.get();

        updateAnimationFSM(ctx, deltaTime);

        renderer.addIm3dDrawCommand([state = this->asRef<RemotePlayerState>(), pos, heading]() {
            auto cameraOffset = Config::PLAYER_CAMERA_OFFSET_Y;

            Im3d::PushDrawState();
            Im3d::SetColor(Im3d::Color{0.0f, 0.0f, 1.0f, 1.0f});
            Im3d::SetSize(5.0f);
            Im3d::DrawCapsule(
                    Im3d::Vec3{pos.x, pos.y - Config::PLAYER_HALF_HEIGHT, pos.z},
                    Im3d::Vec3{pos.x, pos.y + Config::PLAYER_HALF_HEIGHT, pos.z},
                    Config::PLAYER_RADIUS);
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
        m_realVelocity.publish(interpData.velocity);
    }

    void RemotePlayerState::initAnimationFSM() {
        m_animationFSM.addState(
                PlayerAnimations::TPose,
                Animation{"T-Pose", 1},
                [](AnimationFSM<PlayerAnimations>& fsm) {
                    fsm.transitionTo(PlayerAnimations::RifleIdle);
                });
        m_animationFSM.addState(
                PlayerAnimations::RifleIdle,
                Animation{"RifleIdle", 94},
                [](AnimationFSM<PlayerAnimations>& fsm) {
                    if (fsm.getStateArg<bool>("hit", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleIdleHit);
                    } else if (fsm.getStateArg<bool>("jump", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleIdleJump);
                    } else if (fsm.getStateArg<bool>("move_forward", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleRun);
                    } else if (fsm.getStateArg<bool>("move_backward", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleRunBack);
                    }
                });
        m_animationFSM.addStateNoLoop(
                PlayerAnimations::RifleIdleHit,
                Animation{"RifleIdleHit", 14},
                PlayerAnimations::RifleIdle, nullptr);
        m_animationFSM.addStateNoLoop(
                PlayerAnimations::RifleIdleJump,
                Animation{"RifleIdleJump", 19},
                PlayerAnimations::RifleIdle, nullptr);
        m_animationFSM.addState(
                PlayerAnimations::RifleRun,
                Animation{"RifleRun", 23},
                [](AnimationFSM<PlayerAnimations>& fsm) {
                    if (fsm.getStateArg<bool>("hit", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleRunHit);
                    } else if (fsm.getStateArg<bool>("jump", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleRunJump);
                    } else if (!fsm.getStateArg<bool>("move_forward", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleIdle);
                    }
                });
        m_animationFSM.addState(
                PlayerAnimations::RifleRunBack,
                Animation{"RifleRunBack", 16},
                [](AnimationFSM<PlayerAnimations>& fsm) {
                    if (!fsm.getStateArg<bool>("move_backward", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleIdle);
                    }
                    // todo: add hit and jump transitions
                });
        m_animationFSM.addStateNoLoop(
                PlayerAnimations::RifleRunHit,
                Animation{"RifleRunHit", 19},
                PlayerAnimations::RifleRun, nullptr);
        m_animationFSM.addStateNoLoop(
                PlayerAnimations::RifleRunJump,
                Animation{"RifleRunJump", 28},
                PlayerAnimations::RifleRun, nullptr);
    }

}// namespace game::State