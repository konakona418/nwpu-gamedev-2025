#include "State/RemotePlayerState.hpp"

#include "Physics/TypeUtils.hpp"

#include "State/PlayerSharedConfig.hpp"

#include "NetworkDispatcher.hpp"
#include "Registry.hpp"


#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

#include "im3d.h"
#include "imgui.h"

namespace game::State {
    static ParamF WEAPON_MODEL_M4_SCALE("weapon.m4a1.remote.scale", 0.075f);
    static ParamF WEAPON_M4_REMOTE_STANCE_OFFSET_XZ("weapon.m4a1.remote.stance_offset_xz", 0.25f);
    static ParamF WEAPON_M4_REMOTE_STANCE_OFFSET_Y("weapon.m4a1.remote.stance_offset_y", 0.45f);
    static ParamF WEAPON_M4_REMOTE_STANCE_OFFSET_Y_RUN("weapon.m4a1.remote.stance_offset_y_run", 0.30f);
    static ParamF4 WEAPON_M4_ATTACH_POINT_LOCAL_BIAS(
            "weapon.m4a1.attach_point_local_bias",
            ParamFloat4{
                    -0.1f,
                    -0.04f,
                    0.05f,
                    0.0f,
            });
    static ParamF WEAPON_M4_REMOTE_ATTACH_LOCAL_YAW_DEGREES(
            "weapon.m4a1.remote.attach_local_yaw_degrees",
            3.0f);

    static moe::UnorderedMap<moe::String, moe::AnimationId> getAnimationsFromRenderable(GameManager& ctx, moe::RenderableId renderableId) {
        auto scene = ctx.renderer().m_caches.objectCache.get(renderableId).value();
        auto* animatableRenderable = scene->checkedAs<moe::VulkanSkeletalAnimation>(moe::VulkanRenderableFeature::HasSkeletalAnimation).value();
        auto& animations = animatableRenderable->getAnimations();

        return animations;
    }

    void RemotePlayerState::loadAudioSourcesForRemoteFootsteps(GameManager& ctx) {
        auto footstepSoundData = m_playerFootstepSoundLoader.generate();
        if (!footstepSoundData) {
            moe::Logger::error("RemotePlayerState::loadAudioSourcesForRemoteFootsteps: failed to load footstep sound data");
            return;
        }

        m_playerFootstepSoundProvider = moe::makeRef<moe::StaticOggProvider>(footstepSoundData.value());

        // preload audio sources for footsteps
        {
            auto sources = ctx.audio().createAudioSources(PlayerConfig::MAX_SIMULTANEOUS_PLAYER_FOOTSTEPS.get());
            for (auto& source: sources) {
                ctx.audio().loadAudioSource(source, m_playerFootstepSoundProvider, false);
                m_activeLocalFootsteps.push_back(source);
            }
        }
    }

    void RemotePlayerState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering RemotePlayerState, id={}", m_playerTempId);
        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("RemotePlayerState::onEnter: GamePlaySharedData not found in registry");
            return;
        }

        auto playerInfoIt = sharedData->playersByTempId.find(m_playerTempId);
        if (playerInfoIt == sharedData->playersByTempId.end()) {
            moe::Logger::error(
                    "RemotePlayerState::onEnter: "
                    "player info not found for temp id={}",
                    m_playerTempId);
            return;
        }

        moe::Logger::info(
                "RemotePlayerState::onEnter: "
                "player name='{}', team={}",
                utf8::utf32to8(playerInfoIt->second.name),
                playerInfoIt->second.team == GamePlayerTeam::CT ? "CT" : "T");

        // acquire player model from object pool
        if (playerInfoIt->second.team == GamePlayerTeam::T) {
            m_playerModelGuard = std::make_unique<game::ObjectPoolGuard>(moe::asset("assets/models/T-Model.glb"));
            m_playerModel = m_playerModelGuard->getRenderableId();
        } else {
            m_playerModelGuard = std::make_unique<game::ObjectPoolGuard>(moe::asset("assets/models/CT-Model.glb"));
            m_playerModel = m_playerModelGuard->getRenderableId();
        }

        if (m_playerModel == moe::NULL_RENDERABLE_ID) {
            moe::Logger::error("Failed to load player model");
            return;
        }

        m_animationIds = getAnimationsFromRenderable(ctx, m_playerModel);

        m_weaponModel = m_weaponModelLoader.generate().value_or(moe::NULL_RENDERABLE_ID);
        if (m_weaponModel == moe::NULL_RENDERABLE_ID) {
            moe::Logger::error("Failed to load weapon model");
            return;
        }

        loadAudioSourcesForRemoteFootsteps(ctx);
        initAnimationFSM();

        ctx.addDebugDrawFunction(
                fmt::format("RemotePlayerState id={}", m_playerTempId),
                [this]() {
                    ImGui::Begin(fmt::format("RemotePlayerState id={}", m_playerTempId).c_str());
                    auto pos = m_realPosition.get();
                    auto heading = m_realHeading.get();
                    auto velocity = m_realVelocity.get();

                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Remote Player State, id=%d", m_playerTempId);
                    ImGui::Text("Position: (%f, %f, %f)", pos.x, pos.y, pos.z);
                    ImGui::Text("Heading: (%f, %f, %f)", heading.x, heading.y, heading.z);
                    ImGui::Text("Velocity: (%f, %f, %f)", velocity.x, velocity.y, velocity.z);
                    ImGui::Text("Health: %f", m_realHealth.get());

                    auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
                    if (sharedData) {
                        auto playerInfoIt = sharedData->playersByTempId.find(m_playerTempId);
                        if (playerInfoIt != sharedData->playersByTempId.end()) {
                            ImGui::Text("Player Name: %s", utf8::utf32to8(playerInfoIt->second.name).c_str());
                            ImGui::Text("Player Team: %s", playerInfoIt->second.team == GamePlayerTeam::CT ? "CT" : "T");
                        } else {
                            ImGui::Text("Player Info: Not Found");
                        }
                    } else {
                        ImGui::Text("GamePlaySharedData not found in registry!");
                    }

                    ImGui::End();
                });

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<RemotePlayerState>()](moe::PhysicsEngine& physics) mutable {
                    auto& bodyInterface = physics.getPhysicsSystem().GetBodyInterface();

                    auto capsuleShape =
                            JPH::CapsuleShapeSettings(
                                    PlayerConfig::PLAYER_HALF_HEIGHT,
                                    PlayerConfig::PLAYER_RADIUS)
                                    .Create()
                                    .Get();
                    auto characterSettings = JPH::CharacterVirtualSettings();
                    characterSettings.mShape = capsuleShape;
                    // to prevent wall climbing
                    characterSettings.mSupportingVolume =
                            JPH::Plane(
                                    JPH::Vec3::sAxisY(),
                                    PlayerConfig::PLAYER_SUPPORTING_VOLUME_CONSTANT.get());
                    characterSettings.mMaxSlopeAngle =
                            JPH::DegreesToRadians(PlayerConfig::PLAYER_MAX_SLOPE_ANGLE_DEGREES.get());

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
        bool isDying = m_realHealth.get() <= 0.1f;

        m_animationFSM.setStateArg("move_forward", isMovingForward);
        m_animationFSM.setStateArg("move_backward", isMovingBackward);
        m_animationFSM.setStateArg("dying", isDying);
        // todo: set hit and jump args later

        m_animationFSM.update(deltaTime);
        auto animationSample = m_animationFSM.sampleCurrentAnimation();

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        auto computeHandle = renderctx.submitComputeSkin(
                m_playerModel,
                m_animationIds[animationSample.animation.name],
                animationSample.timeInAnimation);

        auto rotation = std::atan2(heading.x, heading.z);
        auto realPosition = m_realPosition.get();
        realPosition.y -= (PlayerConfig::PLAYER_HALF_HEIGHT + PlayerConfig::PLAYER_RADIUS);// offset to feet position

        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::error("RemotePlayerState::updateAnimationFSM: GamePlaySharedData not found");
            return;
        }

        renderctx.submitRender(
                m_playerModel,
                moe::Transform{}
                        .setPosition(realPosition)
                        .setRotation(glm::vec3(0.0f, rotation, 0.0f)),
                computeHandle);
    }

    void RemotePlayerState::renderWeapon(GameManager& ctx) {
        auto health = m_realHealth.get();
        if (health <= 0.1f) {
            return;// do not render weapon if dead
        }

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        auto pos = m_realPosition.get();
        auto heading = m_realHeading.get();
        auto velocity = m_realVelocity.get();

        float bodyRotation = std::atan2(heading.x, heading.z);
        float localYawBias = glm::radians(WEAPON_M4_REMOTE_ATTACH_LOCAL_YAW_DEGREES.get());
        float finalWeaponRotation = bodyRotation + localYawBias;

        auto stanceOffsetXZ = WEAPON_M4_REMOTE_STANCE_OFFSET_XZ.get();
        float stanceOffsetY;
        if (glm::length(glm::vec3(velocity.x, 0.0f, velocity.z)) > 0.1f) {
            stanceOffsetY = WEAPON_M4_REMOTE_STANCE_OFFSET_Y_RUN.get();
        } else {
            stanceOffsetY = WEAPON_M4_REMOTE_STANCE_OFFSET_Y.get();
        }

        glm::vec3 weaponOffset{
                stanceOffsetXZ * heading.x,
                stanceOffsetY,
                stanceOffsetXZ * heading.z,
        };

        auto localBias_ = WEAPON_M4_ATTACH_POINT_LOCAL_BIAS.get();
        glm::vec3 localBias{
                localBias_.x,
                localBias_.y,
                localBias_.z,
        };

        auto bodyRotationMat = glm::rotate(
                glm::mat4(1.0f),
                bodyRotation,
                glm::vec3(0.0f, 1.0f, 0.0f));

        auto rotatedLocalBias = glm::vec3(
                bodyRotationMat * glm::vec4(localBias, 1.0f));

        auto renderPos = pos + weaponOffset + rotatedLocalBias;

        auto finalRotationMat = glm::rotate(
                glm::mat4(1.0f),
                finalWeaponRotation,
                glm::vec3(0.0f, 1.0f, 0.0f));

        renderctx.submitRender(
                m_weaponModel,
                moe::Transform{}
                        .setPosition(renderPos)
                        .setRotation(finalRotationMat)
                        .setScale(glm::vec3(WEAPON_MODEL_M4_SCALE.get())));
    }

    void RemotePlayerState::playRemoteFootstepSoundAtPosition(GameManager& ctx, const glm::vec3& position, float deltaTime) {
        if (m_activeLocalFootsteps.empty() || !m_playerFootstepSoundProvider) {
            return;
        }

        m_footstepSoundCooldownTimer -= deltaTime;
        if (m_footstepSoundCooldownTimer > 0.0f) {
            return;
        }

        auto& audioInterface = ctx.audio();

        auto source = m_activeLocalFootsteps.front();
        m_activeLocalFootsteps.pop_front();

        audioInterface.setAudioSourcePosition(source, position.x, position.y, position.z);
        audioInterface.playAudioSource(source);

        m_activeLocalFootsteps.push_back(std::move(source));

        // reset cooldown timer
        m_footstepSoundCooldownTimer = PlayerConfig::PLAYER_FOOTSTEP_SOUND_COOLDOWN.get();
    }

    void RemotePlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer();

        auto pos = m_realPosition.get();
        auto heading = m_realHeading.get();

        updateAnimationFSM(ctx, deltaTime);
        renderWeapon(ctx);

        // play footstep sound if moving
        auto velocity = m_realVelocity.get();
        glm::vec3 flatVelocity = glm::vec3(velocity.x, 0.0f, velocity.z);
        if (glm::length2(flatVelocity) > 0.01f) {
            playRemoteFootstepSoundAtPosition(ctx, pos, deltaTime);
        }

        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (sharedData) {
            if (sharedData->debugShowPlayerHitboxes) {
                renderer.addIm3dDrawCommand([state = this->asRef<RemotePlayerState>(), pos, heading]() {
                    auto cameraOffset = PlayerConfig::PLAYER_CAMERA_OFFSET_Y;

                    Im3d::PushDrawState();
                    Im3d::SetColor(Im3d::Color{0.0f, 0.0f, 1.0f, 1.0f});
                    Im3d::SetSize(5.0f);
                    Im3d::DrawCapsule(
                            Im3d::Vec3{pos.x, pos.y - PlayerConfig::PLAYER_HALF_HEIGHT, pos.z},
                            Im3d::Vec3{pos.x, pos.y + PlayerConfig::PLAYER_HALF_HEIGHT, pos.z},
                            PlayerConfig::PLAYER_RADIUS);
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
        }
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
            motionData.health = update->health;

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
        m_realHealth.publish(interpData.health);
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
                    } else if (fsm.getStateArg<bool>("dying", false)) {
                        fsm.transitionTo(PlayerAnimations::Dying);
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
                    } else if (fsm.getStateArg<bool>("dying", false)) {
                        fsm.transitionTo(PlayerAnimations::Dying);
                    }
                });
        m_animationFSM.addState(
                PlayerAnimations::RifleRunBack,
                Animation{"RifleRunBack", 16},
                [](AnimationFSM<PlayerAnimations>& fsm) {
                    if (!fsm.getStateArg<bool>("move_backward", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleIdle);
                    } else if (fsm.getStateArg<bool>("dying", false)) {
                        fsm.transitionTo(PlayerAnimations::Dying);
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
        m_animationFSM.addState(
                PlayerAnimations::Dying,
                Animation{"Dying", 65535},// let it stay at last frame
                [](AnimationFSM<PlayerAnimations>& fsm) {
                    if (!fsm.getStateArg<bool>("dying", false)) {
                        fsm.transitionTo(PlayerAnimations::RifleIdle);
                    }
                });
    }

}// namespace game::State