#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"
#include "InterpolationBuffer.hpp"

#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"

#include "State/GamePlayData.hpp"

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
        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;

        moe::UniquePtr<InterpolationBuffer<RemotePlayerMotionInterpolationData>> m_motionInterpolationBuffer =
                std::make_unique<InterpolationBuffer<RemotePlayerMotionInterpolationData>>();
    };
}// namespace game::State