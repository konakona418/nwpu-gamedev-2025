#pragma once

#include "GameState.hpp"
#include "Input.hpp"
#include "Param.hpp"

#include "Core/AFlag.hpp"
#include "Core/DBuffer.hpp"
#include "Core/Deferred.hpp"

#include "Math/Common.hpp"


#include "Physics/JoltIncludes.hpp"

#include "Jolt/Physics/Character/CharacterVirtual.h"


namespace game::State {
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
        moe::AFlag m_jumpRequested;
        moe::DBuffer<glm::vec3> m_realPosition;
        moe::Deferred<JPH::Ref<JPH::CharacterVirtual>> m_character;

        InputProxy m_inputProxy{InputProxy::PRIORITY_DEFAULT};
    };
}// namespace game::State