#include "State/LocalPlayerState.hpp"

#include "State/GamePlayData.hpp"
#include "State/PauseUIState.hpp"
#include "State/PlayerSharedConfig.hpp"

#include "GameManager.hpp"
#include "InputUtil.hpp"
#include "NetworkAdaptor.hpp"
#include "NetworkDispatcher.hpp"
#include "Param.hpp"
#include "Registry.hpp"

#include "Physics/TypeUtils.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

#include "FlatBuffers/Generated/Sent/receiveGamingPacket_generated.h"


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
        input.addProxy(&m_inputProxy);

#define X(name, key) input.addKeyMapping(#name, key);
        PLAYER_KEY_MAPPING_XXX()
#undef X

        input.addKeyEventMapping("escape_player", GLFW_KEY_ESCAPE);
        m_inputProxy.setMouseState(false);// disable mouse cursor

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

    void LocalPlayerState::onExit(GameManager& ctx) {
        auto& input = ctx.input();

        m_inputProxy.setMouseState(true);// enable mouse cursor

#define X(name, key) input.removeKeyMapping(#name);
        PLAYER_KEY_MAPPING_XXX()
#undef X

        input.removeProxy(&m_inputProxy);

        ctx.physics().dispatchOnPhysicsThread(
                [state = this->asRef<LocalPlayerState>()](moe::PhysicsEngine& physics) mutable {
                    state->m_character.set(nullptr);// release character
                });
    }

    void LocalPlayerState::constructOpenFireEventAndSend(GameManager& ctx, const glm::vec3& position, const glm::vec3& direction) {
        // construct OpenFire event
        flatbuffers::FlatBufferBuilder fbb;

        auto sharedData = Registry::getInstance().get<GamePlaySharedData>();
        if (!sharedData) {
            moe::Logger::warn(
                    "LocalPlayerState::constructOpenFireEventAndSend: "
                    "no GamePlaySharedData found in registry");
            return;
        }

        // todo: weapon slot id later
        // !!
        auto firePkt = myu::net::CreateFirePacket(
                fbb,
                sharedData->playerTempId,
                position.x, position.y, position.z,
                direction.x, direction.y, direction.z,
                myu::net::WeaponSlot::SLOT_SECONDARY,
                sharedData->playerFireSequence++);

        auto timePack = game::Util::getTimePack();
        auto header = myu::net::CreatePacketHeader(
                fbb,
                timePack.physicsTick,
                timePack.currentTimeMillis);

        auto message = myu::net::CreateNetMessage(
                fbb,
                header,
                myu::net::PacketUnion::FirePacket,
                firePkt.Union());

        fbb.Finish(message);

        auto dataSpan = fbb.GetBufferSpan();
        ctx.network().sendData(dataSpan, true);//reliable
    }

    void LocalPlayerState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& cam = ctx.renderer().getDefaultCamera();

        glm::vec3 dir{0.0f};
        float yawDelta = 0.0f;
        float pitchDelta = 0.0f;

        if (m_inputProxy.isValid()) {
            // only process input if we have the input lock

            MovementHelper movementHelper;
#define X(name, key) movementHelper.name = m_inputProxy.isKeyPressed(#name)
            PLAYER_KEY_MAPPING_XXX()
#undef X

            // ! this still needs further check,
            // ! if the logic is incorrect, it may cause some weird movement issues
            // ! after synchronization from server is implemented
            auto front = cam.getFront();
            front.y = 0;// proj to xOz
            front = glm::normalize(front);
            dir = movementHelper.realMovementVecFPS(front, cam.getRight(), cam.getUp());

            auto [mouseX, mouseY] = m_inputProxy.getMouseDelta();
            yawDelta = mouseX * PLAYER_MOUSE_SENSITIVITY;
            pitchDelta = -mouseY * PLAYER_MOUSE_SENSITIVITY;// invert Y axis

            // if escape is just released, show menu
            if (m_inputProxy.isKeyJustReleased("escape_player")) {
                this->addChildState(moe::Ref(new State::PauseUIState()));
            }

            if (m_inputProxy.getMouseButtonState().pressedLMB) {
                constexpr float PLAYER_OPEN_FIRE_COOLDOWN = 0.5f;// secs
                m_openFireCooldownTimer -= deltaTime;
                if (m_openFireCooldownTimer <= 0.0f) {
                    // can open fire
                    auto camPos = cam.getPosition();
                    auto camFront = cam.getFront();

                    constructOpenFireEventAndSend(ctx, camPos, camFront);

                    moe::Logger::debug("LocalPlayerState: Open fire event sent, pos=({}, {}, {}), dir=({}, {}, {})",
                                       camPos.x, camPos.y, camPos.z,
                                       camFront.x, camFront.y, camFront.z);

                    m_openFireCooldownTimer = PLAYER_OPEN_FIRE_COOLDOWN;
                }
            }
        }

        auto pos = m_realPosition.get();

        // offset camera position from mass center to eye level
        pos.y += PLAYER_CAMERA_OFFSET_Y;

        cam.setPosition(pos);

        float newYaw = cam.getYaw() + yawDelta;
        cam.setYaw(newYaw);

        float newPitch = cam.getPitch() + pitchDelta;
        cam.setPitch(newPitch);

        if (dir.y > 0.001f) {
            m_jumpRequested.set();
        }
        // todo: implement crouch later

        m_movingDirection.publish(std::move(dir));
        m_lookingYawDegrees.publish(newYaw);
        m_lookingPitchDegrees.publish(newPitch);
    }

    void LocalPlayerState::handleCharacterUpdate(
            JPH::Ref<JPH::CharacterVirtual> character,
            const glm::vec3& inputIntention, bool jumpRequested,
            float deltaTime,
            GameManager& ctx) {
        auto dir = inputIntention;

        auto& physicsSystem = ctx.physics().getPhysicsSystem();

        auto velocity = dir * PLAYER_SPEED.get();

        auto lastVel = character->GetLinearVelocity();

        auto velXoZ = moe::Physics::toJoltType<JPH::Vec3>(
                glm::vec3(velocity.x, 0, velocity.z));
        float velY = 0.0f;

        auto groundState = character->GetGroundState();

        bool onGround = groundState == JPH::CharacterVirtual::EGroundState::OnGround;
        bool onSteepGround = groundState == JPH::CharacterVirtual::EGroundState::OnSteepGround;// steep slope

        auto gravity = -character->GetUp() * physicsSystem.GetGravity().Length();
        if (onGround) {
            if (jumpRequested) {
                // apply jump impulse
                moe::Logger::debug("LocalPlayerState: Jump requested");
                velY = PLAYER_JUMP_VELOCITY.get();
            } else {
                // on ground, preserve horizontal velocity, reset vertical velocity
                velY = 0.0f;
            }
            // todo: apply ground friction later
        } else if (onSteepGround) {
            // on steep ground, slide down
            JPH::Vec3 groundNormal = character->GetGroundNormal();
            auto velocity = velXoZ;

            float dot = velocity.Dot(groundNormal);
            if (dot < 0.0f) {
                // remove component against ground normal
                velocity -= groundNormal * dot;
            }

            velXoZ = velocity;

            velY = lastVel.GetY() + gravity.GetY() * deltaTime;
        } else {
            // in air, preserve Y velocity
            // ! fixme: players can still move freely in air, need to limit air control later
            velY = lastVel.GetY() + gravity.GetY() * deltaTime;
        }

        JPH::Vec3 finalVel(velXoZ.GetX(), velY, velXoZ.GetZ());

        // check collision
        JPH::CharacterVirtual::ExtendedUpdateSettings settings;
        // todo: fill in this settings, to allow character to climb
        if (!onGround) {
            settings.mStickToFloorStepDown = JPH::Vec3::sZero();// disable stick to floor when in air
            settings.mWalkStairsStepUp = JPH::Vec3::sZero();    // disable walk stairs when in air
        } else {
            // if on ground, enable stick to floor and walk stairs
            // ! fixme: this still needs tuning
            settings.mStickToFloorStepDown =
                    JPH::Vec3(
                            0,
                            PLAYER_STICK_TO_FLOOR_STEP_DOWN.get(),
                            0);

            settings.mWalkStairsStepUp =
                    JPH::Vec3(
                            0,
                            PLAYER_WALK_STAIRS_STEP_UP.get(),
                            0);
        }

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
    }

    void LocalPlayerState::replayPositionUpdatesUpToTick(GameManager& ctx, JPH::Ref<JPH::CharacterVirtual> character) {
        bool positionShouldUpdate = false;
        uint64_t serverPhysicsTick = 0;

        auto sync = syncPositionWithServer(ctx, character->GetPosition(), positionShouldUpdate, serverPhysicsTick);
        if (!positionShouldUpdate) {
            return;
        }

        moe::Logger::debug(
                "LocalPlayerState: Syncing position with server to ({}, {}, {})",
                sync.GetX(),
                sync.GetY(),
                sync.GetZ());
        size_t replayBeginIndex = 0;
        for (size_t i = 0; i < m_positionInterpolationBuffer->size(); ++i) {
            auto& data = (*m_positionInterpolationBuffer)[i];
            if (data.physicsTick > serverPhysicsTick) {
                replayBeginIndex = i + 1;
            } else {
                break;
            }
        }

        character->SetPosition(sync);
        if (replayBeginIndex == 0) {
            // no need to replay
            return;
        }

        if (replayBeginIndex > 16) {
            moe::Logger::warn(
                    "LocalPlayerState: Large number of position updates to replay: {}, may cause noticeable correction",
                    replayBeginIndex);
            return;
        }

        moe::Logger::debug(
                "LocalPlayerState: Replaying {} physics updates after sync; starting from index {}, tick {}",
                replayBeginIndex + 1, replayBeginIndex, serverPhysicsTick);

        for (int i = replayBeginIndex; i > 0; --i) {
            auto& data = (*m_positionInterpolationBuffer)[i];

            handleCharacterUpdate(
                    character,
                    data.inputIntention,
                    data.jumpRequested,
                    data.deltaTime,
                    ctx);
        }
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

        auto dir = m_movingDirection.get();
        float yawDegrees = m_lookingYawDegrees.get();// these two are for sending to server later
        float pitchDegrees = m_lookingPitchDegrees.get();

        auto jumpRequested = m_jumpRequested.consume();

        // no matter what, the state of m_jumpRequested is consumed here
        // otherwise, if in some cases the jump request is never processed,
        // the state will remain true and cause unwanted jumps later
        constructMovementUpdateAndSend(ctx, dir, yawDegrees, pitchDegrees);

        m_positionInterpolationBuffer->pushBack(
                PlayerStateInterpolationData{
                        .inputIntention = dir,
                        .jumpRequested = jumpRequested,
                        .deltaTime = deltaTime,
                        .physicsTick = ctx.physics().getCurrentTickIndex(),
                });

        // sync position with server
        replayPositionUpdatesUpToTick(ctx, character);

        handleCharacterUpdate(
                character,
                dir,
                jumpRequested,
                deltaTime,
                ctx);

        m_realPosition.publish(moe::Physics::fromJoltType<glm::vec3>(character->GetPosition()));
    }

    void LocalPlayerState::constructMovementUpdateAndSend(
            GameManager& ctx,
            const glm::vec3& dir,
            float yawDeg, float pitchDeg) {
        // don't send if network is not running
        if (!ctx.network().isConnected()) {
            return;
        }

        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        if (!gamePlaySharedData) {
            return;
        }

        // not registered yet
        if (gamePlaySharedData->playerTempId == INVALID_PLAYER_TEMP_ID) {
            return;
        }

        flatbuffers::FlatBufferBuilder fbb;
        auto movePkt = myu::net::CreateMovePacket(
                fbb,
                gamePlaySharedData->playerTempId,
                dir.x, dir.y, dir.z,
                glm::radians(yawDeg),// require radians
                glm::radians(pitchDeg),
                false, false, false,
                gamePlaySharedData->playerMoveSequence++);// increment after use

        auto time = game::Util::getTimePack();
        auto header = myu::net::CreatePacketHeader(
                fbb,
                time.physicsTick,
                time.currentTimeMillis);

        auto msg = myu::net::CreateNetMessage(
                fbb,
                header,
                myu::net::PacketUnion::MovePacket,
                movePkt.Union());

        fbb.Finish(msg);

        // send via unreliable channel
        ctx.network().sendData(fbb.GetBufferSpan(), false);
    }

    JPH::Vec3 LocalPlayerState::syncPositionWithServer(GameManager& ctx, JPH::Vec3 currentPos, bool& outPositionShouldUpdate, uint64_t& outServerPhysicsTick) {
        // don't recv if network is not running
        if (!ctx.network().isConnected()) {
            outPositionShouldUpdate = false;
            return {};
        }

        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();
        if (!gamePlaySharedData) {
            outPositionShouldUpdate = false;
            return {};
        }

        // not registered
        if (gamePlaySharedData->playerTempId == INVALID_PLAYER_TEMP_ID) {
            outPositionShouldUpdate = false;
            return {};
        }

        auto networkDispatcher = gamePlaySharedData->networkDispatcher;

        moe::Optional<NetworkDispatcher::PlayerUpdateData> lastPlayerUpdate;
        while (auto playerUpdate = networkDispatcher->getPlayerUpdate(gamePlaySharedData->playerTempId)) {
            if (!playerUpdate.has_value()) {
                // no more updates
                break;
            }

            lastPlayerUpdate = playerUpdate;
        }

        if (!lastPlayerUpdate.has_value()) {
            // no update received
            outPositionShouldUpdate = false;
            return {};
        }

        if (m_localPlayerSyncCounter++ % LOCAL_PLAYER_SYNC_RATE != 0) {
            outPositionShouldUpdate = false;
            return {};
        }

        auto requestedDelta = lastPlayerUpdate->position - moe::Physics::fromJoltType<glm::vec3>(currentPos);
        if (glm::length2(requestedDelta) < 0.01f * 0.01f) {// 0.05m threshold
            outPositionShouldUpdate = false;
            return {};
        }

        outPositionShouldUpdate = true;
        outServerPhysicsTick = lastPlayerUpdate->physicsTick;
        return moe::Physics::toJoltType<JPH::Vec3>(lastPlayerUpdate->position);
    }

#undef PLAYER_KEY_MAPPING_XXX
}// namespace game::State