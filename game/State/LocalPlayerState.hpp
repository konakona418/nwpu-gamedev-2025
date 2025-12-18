#pragma once

#include "GameState.hpp"

#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"
#include "Math/Common.hpp"


#include "Physics/JoltIncludes.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"


namespace game::State {
    struct LocalPlayerState : public GameState {
    public:
        static constexpr float PLAYER_SPEED = 3.0f;
        static constexpr float PLAYER_ROTATION_SPEED = 0.1f;

        static constexpr float PLAYER_HALF_HEIGHT = 0.8f;
        static constexpr float PLAYER_RADIUS = 0.4f;

        const moe::StringView getName() const override {
            return "LocalPlayerState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        void onPhysicsUpdate(GameManager& ctx, float deltaTime) override;

    private:
        moe::DBuffer<glm::vec3> m_movingDirection;
        moe::DBuffer<glm::vec3> m_realPosition;
        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;
    };
}// namespace game::State