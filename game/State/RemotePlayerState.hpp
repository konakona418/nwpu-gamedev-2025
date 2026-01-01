#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"
#include "InterpolationBuffer.hpp"

#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"

#include "State/GamePlayData.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

#include "AnimationFSM.hpp"
#include "AnyCache.hpp"
#include "ModelLoader.hpp"

#include "Audio/StaticOggProvider.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"

namespace game::State {
    struct RemotePlayerState : public GameState {
    public:
        struct RemotePlayerMotionInterpolationData {
            glm::vec3 position;
            glm::vec3 velocity;
            glm::vec3 heading;
            float health;

            static RemotePlayerMotionInterpolationData interpolate(
                    const RemotePlayerMotionInterpolationData& a,
                    const RemotePlayerMotionInterpolationData& b,
                    float factor) {
                RemotePlayerMotionInterpolationData result;
                result.position = glm::mix(a.position, b.position, factor);
                result.velocity = glm::mix(a.velocity, b.velocity, factor);
                result.heading = glm::mix(a.heading, b.heading, factor);
                result.health = glm::mix(a.health, b.health, factor);
                return result;
            }
        };

        const moe::StringView getName() const override {
            return "RemotePlayerState";
        }

        explicit RemotePlayerState(uint16_t playerTempId)
            : m_playerTempId(playerTempId) {}

        void onEnter(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        void onPhysicsUpdate(GameManager& ctx, float deltaTime) override;
        void onExit(GameManager& ctx) override;

    private:
        using ModelLoader = moe::Preload<moe::Secure<game::AnyCacheLoader<game::ModelLoader>>>;

        uint16_t m_playerTempId{INVALID_PLAYER_TEMP_ID};

        moe::DBuffer<glm::vec3> m_realPosition;
        moe::DBuffer<glm::vec3> m_realHeading;
        moe::DBuffer<glm::vec3> m_realVelocity;
        moe::DBuffer<float> m_realHealth;
        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;

        moe::UniquePtr<InterpolationBuffer<RemotePlayerMotionInterpolationData>> m_motionInterpolationBuffer =
                std::make_unique<InterpolationBuffer<RemotePlayerMotionInterpolationData>>();

        moe::RenderableId m_playerModel{moe::NULL_RENDERABLE_ID};

        // this assumes all animations are the same across all remote players
        moe::UnorderedMap<moe::String, moe::AnimationId> m_animationIds;

        // todo: currently only M4
        ModelLoader m_weaponModelLoader{
                ModelLoaderParam{moe::asset("assets/models/M4A1.glb")},
        };
        moe::RenderableId m_weaponModel{moe::NULL_RENDERABLE_ID};

        enum class PlayerAnimations {
            TPose,

            Idle,

            RifleIdle,
            RifleIdleHit,
            RifleIdleJump,

            RifleRun,
            RifleRunHit,
            RifleRunJump,

            RifleRunBack,

            Dying,
        };

        AnimationFSM<PlayerAnimations> m_animationFSM{PlayerAnimations::TPose};

        moe::Preload<moe::Launch<moe::BinaryLoader>> m_playerFootstepSoundLoader{
                moe::BinaryFilePath(moe::asset("assets/audio/footstep.ogg")),
        };
        moe::Ref<moe::StaticOggProvider> m_playerFootstepSoundProvider{nullptr};
        moe::Deque<moe::Ref<moe::AudioSource>> m_activeLocalFootsteps;
        float m_footstepSoundCooldownTimer{0.0f};

        void loadAudioSourcesForRemoteFootsteps(GameManager& ctx);

        void playRemoteFootstepSoundAtPosition(GameManager& ctx, const glm::vec3& position, float deltaTime);

        void updateAnimationFSM(GameManager& ctx, float deltaTime);
        void renderWeapon(GameManager& ctx);

        void initAnimationFSM();
    };
}// namespace game::State