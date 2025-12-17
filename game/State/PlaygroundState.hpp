#pragma once

#include "GameState.hpp"

#include "Core/Deferred.hpp"
#include "Physics/JoltIncludes.hpp"
#include "Render/Common.hpp"

namespace game::State {
    struct PlaygroundState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "PlaygroundState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        // void onPhysicsUpdate(GameManager& ctx, float deltaTime) override;

    private:
        moe::RenderableId m_playgroundRenderable;
        moe::Deferred<JPH::BodyID> m_playgroundBody;
    };
}// namespace game::State