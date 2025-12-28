#pragma once

#include "GameState.hpp"
#include "Input.hpp"
#include "RingBuffer.hpp"

#include "Core/AFlag.hpp"
#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"

#include "Math/Common.hpp"


#include "Physics/JoltIncludes.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"


namespace game::State {

    struct PlayerStateInterpolationData {
        glm::vec3 inputIntention;
        bool jumpRequested{false};
        float deltaTime{0.0f};
        uint64_t physicsTick{0};
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

        moe::UniquePtr<game::RingBuffer<PlayerStateInterpolationData, 32>> m_positionInterpolationBuffer =
                std::make_unique<game::RingBuffer<PlayerStateInterpolationData, 32>>();
        static constexpr size_t LOCAL_PLAYER_SYNC_RATE = 60;// force position sync every 5 updates ~ 1/12s
        size_t m_localPlayerSyncCounter{0};

        void constructMovementUpdateAndSend(GameManager& ctx, const glm::vec3& dir, float yawDeg, float pitchDeg);

        JPH::Vec3 syncPositionWithServer(GameManager& ctx, JPH::Vec3 currentPos, bool& outPositionShouldUpdate, uint64_t& outServerPhysicsTick);

        void handleCharacterUpdate(
                JPH::Ref<JPH::CharacterVirtual> character,
                const glm::vec3& inputIntention, bool jumpRequested,
                float deltaTime,
                GameManager& ctx);

        void replayPositionUpdatesUpToTick(GameManager& ctx, JPH::Ref<JPH::CharacterVirtual> character);
    };
}// namespace game::State