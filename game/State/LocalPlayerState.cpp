#include "State/LocalPlayerState.hpp"

#include "State/GamePlayData.hpp"
#include "State/PauseUIState.hpp"

#include "GameManager.hpp"
#include "InputUtil.hpp"
#include "NetworkAdaptor.hpp"
#include "Param.hpp"
#include "Registry.hpp"

#include "Physics/TypeUtils.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

#include "FlatBuffers/Generated/Sent/receiveGamingPacket_generated.h"


namespace game::State {
    static ParamF PLAYER_SPEED("player.speed", 3.0f);
    static ParamF PLAYER_JUMP_VELOCITY("player.jump_velocity", 5.0f);

    static ParamF PLAYER_MOUSE_SENSITIVITY("player.rotation_speed", 0.1f, ParamScope::UserConfig);

    static ParamF PLAYER_HALF_HEIGHT("player.half_height", 0.8f);
    static ParamF PLAYER_RADIUS("player.radius", 0.4f);

    // offset from mass center (half height) to camera position (eye level)
    static ParamF PLAYER_CAMERA_OFFSET_Y("player.camera_offset_y", 0.6f);

    static ParamF PLAYER_SUPPORTING_VOLUME_CONSTANT("player.supporting_volume_constant", -0.5f);
    static ParamF PLAYER_MAX_SLOPE_ANGLE_DEGREES("player.max_slope_angle_degrees", 45.0f);

    static ParamF PLAYER_STICK_TO_FLOOR_STEP_DOWN("player.stick_to_floor_step_down", -0.4f);
    static ParamF PLAYER_WALK_STAIRS_STEP_UP("player.walk_stairs_step_up", 0.4f);

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
        float yawDegrees = m_lookingYawDegrees.get();// these two are for sending to server later
        float pitchDegrees = m_lookingPitchDegrees.get();

        auto velocity = dir * PLAYER_SPEED.get();

        auto lastVel = character->GetLinearVelocity();

        auto velXoZ = moe::Physics::toJoltType<JPH::Vec3>(
                glm::vec3(velocity.x, 0, velocity.z));
        float velY = 0.0f;

        // no matter what, the state of m_jumpRequested is consumed here
        // otherwise, if in some cases the jump request is never processed,
        // the state will remain true and cause unwanted jumps later
        bool jumpRequested = m_jumpRequested.consume();

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

        m_realPosition.publish(moe::Physics::fromJoltType<glm::vec3>(character->GetPosition()));

        constructMovementUpdateAndSend(ctx, dir, yawDegrees, pitchDegrees);
    }

    void LocalPlayerState::constructMovementUpdateAndSend(
            GameManager& ctx,
            const glm::vec3& dir,
            float yawDeg, float pitchDeg) {
        // don't send if network is not running
        if (!ctx.network().isRunning()) {
            return;
        }

        auto gamePlaySharedData =
                Registry::getInstance().get<GamePlaySharedData>();

        MOE_ASSERT(
                gamePlaySharedData->playerTempId != INVALID_PLAYER_TEMP_ID,
                "LocalPlayerState::constructMovementUpdateAndSend: invalid player temp id");

        flatbuffers::FlatBufferBuilder fbb;
        auto movePkt = myu::net::CreateMovePacket(
                fbb,
                gamePlaySharedData->playerTempId,
                dir.x, dir.y, dir.z,
                glm::radians(yawDeg),// require radians
                glm::radians(pitchDeg),
                false, false, false,
                m_movementSequenceNumber++);// increment after use

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

#undef PLAYER_KEY_MAPPING_XXX
}// namespace game::State