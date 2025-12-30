#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"
#include "InterpolationBuffer.hpp"

#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"

#include "State/GamePlayData.hpp"

#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

#include "AnimationFSM.hpp"
#include "AnyCache.hpp"
#include "ModelLoader.hpp"


#include "Jolt/Physics/Character/CharacterVirtual.h"

namespace game::State {
    struct RemotePlayerState : public GameState {
    public:
        struct RemotePlayerMotionInterpolationData {
            glm::vec3 position;
            glm::vec3 velocity;
            glm::vec3 heading;

            static RemotePlayerMotionInterpolationData interpolate(
                    const RemotePlayerMotionInterpolationData& a,
                    const RemotePlayerMotionInterpolationData& b,
                    float factor) {
                RemotePlayerMotionInterpolationData result;
                result.position = glm::mix(a.position, b.position, factor);
                result.velocity = glm::mix(a.velocity, b.velocity, factor);
                result.heading = glm::mix(a.heading, b.heading, factor);
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
        uint16_t m_playerTempId{INVALID_PLAYER_TEMP_ID};

        moe::DBuffer<glm::vec3> m_realPosition;
        moe::DBuffer<glm::vec3> m_realHeading;
        moe::DBuffer<glm::vec3> m_realVelocity;
        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;

        moe::UniquePtr<InterpolationBuffer<RemotePlayerMotionInterpolationData>> m_motionInterpolationBuffer =
                std::make_unique<InterpolationBuffer<RemotePlayerMotionInterpolationData>>();

        // todo: distinguish CT and T models
        moe::Preload<moe::Secure<game::AnyCacheLoader<game::ModelLoader>>> m_terroristModelLoader{
                ModelLoaderParam{moe::asset("assets/models/Terrorist-Model.glb")}};
        moe::RenderableId m_terroristModel{moe::NULL_RENDERABLE_ID};
        moe::UnorderedMap<moe::String, moe::AnimationId> m_terroristAnimationIds;

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
        };

        AnimationFSM<PlayerAnimations> m_animationFSM{PlayerAnimations::TPose};

        void updateAnimationFSM(GameManager& ctx, float deltaTime);

        void initAnimationFSM();
    };
}// namespace game::State