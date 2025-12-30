#pragma once

#include "GameState.hpp"
#include "Input.hpp"
#include "RingBuffer.hpp"

#include "State/BombPlantState.hpp"
#include "State/GameCommon.hpp"
#include "State/HudState.hpp"

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

        void setValid(bool valid) {
            m_valid = valid;
        }

        bool isValid() const {
            return m_valid;
        }

    private:
        moe::DBuffer<glm::vec3> m_movingDirection;
        moe::DBuffer<float> m_lookingYawDegrees;
        moe::DBuffer<float> m_lookingPitchDegrees;

        moe::AFlag m_jumpRequested;

        moe::DBuffer<glm::vec3> m_realPosition;

        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;
        InputProxy m_inputProxy{InputProxy::PRIORITY_DEFAULT};

        moe::UniquePtr<game::RingBuffer<PlayerStateInterpolationData, 32>> m_positionInterpolationBuffer =
                std::make_unique<game::RingBuffer<PlayerStateInterpolationData, 32>>();
        static constexpr size_t LOCAL_PLAYER_SYNC_RATE = 60;// force position sync every 60 updates
        size_t m_localPlayerSyncCounter{0};

        float m_openFireCooldownTimer{0.0f};
        WeaponSlot m_currentWeaponSlot{WeaponSlot::Secondary};

        moe::Ref<HudState> m_hudState{nullptr};
        moe::DBuffer<float> m_healthBuffer;

        moe::Ref<BombPlantState> m_bombPlantState{nullptr};
        moe::UnorderedMap<BombSite, BombSiteInfo> m_bombsiteInfoMap;
        bool m_debugShowBombsiteRadius{false};

        // by default, the local player state is invalid until set otherwise
        bool m_valid{false};

        // physics
        void constructMovementUpdateAndSend(GameManager& ctx, const glm::vec3& dir, float yawDeg, float pitchDeg);

        // physics
        JPH::Vec3 syncPositionWithServer(GameManager& ctx, JPH::Vec3 currentPos, bool& outPositionShouldUpdate, uint64_t& outServerPhysicsTick);

        // physics
        void handleCharacterUpdate(
                JPH::Ref<JPH::CharacterVirtual> character,
                const glm::vec3& inputIntention, bool jumpRequested,
                float deltaTime,
                GameManager& ctx);

        // render
        void handleHudUpdate(GameManager& ctx);
        void handleMotionStateUpdate(GameManager& ctx, float deltaTime);
        void handleInputStateUpdate(GameManager& ctx, float deltaTime);
        void renderDebugBombsiteRadius(GameManager& ctx);

        // physics
        void replayPositionUpdatesUpToTick(GameManager& ctx, JPH::Ref<JPH::CharacterVirtual> character);

        // render
        void constructOpenFireEventAndSend(GameManager& ctx, const glm::vec3& position, const glm::vec3& direction);

        // render
        void plantBombAtBombsite(GameManager& ctx, BombSite site);

        // render
        void defuseBomb(GameManager& ctx);
    };
}// namespace game::State