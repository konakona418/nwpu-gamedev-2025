#pragma once

#include "GameState.hpp"
#include "Input.hpp"
#include "InterpolationBuffer.hpp"

#include "Core/AFlag.hpp"
#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"

#include "Math/Common.hpp"


#include "Physics/JoltIncludes.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"


namespace game::State {

    struct PlayerStateInterpolationData {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        glm::vec3 heading{0.0f};// pitch vec on xOz plane

        static PlayerStateInterpolationData interpolate(
                const PlayerStateInterpolationData& a,
                const PlayerStateInterpolationData& b,
                float t) {
            PlayerStateInterpolationData result;
            result.position = glm::mix(a.position, b.position, t);
            result.velocity = glm::mix(a.velocity, b.velocity, t);
            result.heading = glm::mix(a.heading, b.heading, t);
            return result;
        }
    };

    struct LocalPlayerState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "LocalPlayerState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        void onPhysicsUpdate(GameManager& ctx, float deltaTime) override;

    private:
        moe::DBuffer<glm::vec3> m_movingDirection;
        moe::DBuffer<float> m_lookingYawDegrees;
        moe::DBuffer<float> m_lookingPitchDegrees;

        moe::AFlag m_jumpRequested;

        moe::DBuffer<glm::vec3> m_realPosition;

        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;
        InputProxy m_inputProxy{InputProxy::PRIORITY_DEFAULT};

        uint32_t m_movementSequenceNumber{0};

        game::InterpolationBuffer<PlayerStateInterpolationData> m_positionInterpolationBuffer{};
        static constexpr size_t LOCAL_PLAYER_SYNC_RATE = 20;// force position sync every 20 updates ~ 1/3s
        size_t m_localPlayerSyncCounter{0};
        // difference to apply when syncing is requested
        // should be interpolated over time, and not applied instantly!!!
        glm::vec3 m_interpolateRequestedDelta{0.0f};

        void constructMovementUpdateAndSend(GameManager& ctx, const glm::vec3& dir, float yawDeg, float pitchDeg);

        void syncPositionWithServer(GameManager& ctx);
    };
}// namespace game::State