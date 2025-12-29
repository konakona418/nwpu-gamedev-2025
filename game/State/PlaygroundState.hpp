#pragma once

#include "GameState.hpp"

#include "Core/Deferred.hpp"
#include "Physics/JoltIncludes.hpp"
#include "Render/Common.hpp"

#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

#include "AnyCache.hpp"
#include "ModelLoader.hpp"


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
        moe::Preload<moe::Secure<game::AnyCacheLoader<game::ModelLoader>>> m_playgroundModelLoader{
                ModelLoaderParam{moe::asset("assets/models/playground.glb")}};

        moe::RenderableId m_playgroundRenderable{moe::NULL_RENDERABLE_ID};
        moe::Deferred<JPH::BodyID> m_playgroundBody;
    };
}// namespace game::State