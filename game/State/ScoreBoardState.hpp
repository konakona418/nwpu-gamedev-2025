#pragma once

#include "GameState.hpp"

#include "State/GamePlayData.hpp"

#include "UI/BoxWidget.hpp"
#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkBoxWidget.hpp"
#include "UI/Vulkan/VkImageWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "GPUImageLoader.hpp"
#include "Tween.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/ImageLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"


namespace game::State {
    class ScoreBoardState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "ScoreBoardState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;

        void onUpdate(GameManager& ctx, float deltaTime) override;

        // call this when a team wins a round to update the score
        void updateScore(GamePlayerTeam winningTeam);
        // call this when a player dies, to update the remaining player count
        void updateRemainingPlayers(uint16_t whoDied);
        // call this when bomb is planted or defused
        void updateBombStatus(bool isPlanted);

        // call this before a new round starts to reset the scores and player counts
        void resetRound();

        void setCountDownTime(uint32_t msLeft);

    private:
        using FontLoaderT = moe::Secure<game::AnyCacheLoader<game::FontLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;
        using ImageLoaderT = moe::Secure<game::AnyCacheLoader<game::GPUImageLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<
                        moe::ImageLoader<moe::BinaryLoader>>>>>>>;

        FontLoaderT m_fontLoader{
                FontLoaderParam{24.0f},
                moe::BinaryFilePath(moe::asset("assets/fonts/orbitron/Orbitron-Medium.ttf"))};
        moe::FontId m_fontId{moe::NULL_FONT_ID};

        ImageLoaderT m_bombIconLoader{moe::BinaryFilePath(moe::asset("assets/images/bomb.png"))};
        moe::ImageId m_bombIconId{moe::NULL_IMAGE_ID};

        uint16_t m_counterTerroristsScore{0};
        uint16_t m_terroristsScore{0};

        uint16_t m_remainingTerrorists{0};
        uint16_t m_remainingCounterTerrorists{0};

        bool m_isBombPlanted{false};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_topContainer{nullptr};

        float m_countDownTimeSeconds{0.0f};
        float m_countDownDeltaAccum{0.0f};
        moe::Ref<moe::VkTextWidget> m_countDownTextWidget{nullptr};

        Tween<QuadraticProvider, QuadraticProvider> m_bombIconTransparencyTween{0.5f, 0.5f};
        float m_timeAccum{0.0f};
        moe::Ref<moe::VkImageWidget> m_bombStatusIconWidget{nullptr};

        moe::Ref<moe::BoxWidget> m_scoreBoardSet{nullptr};
        moe::Ref<moe::VkTextWidget> m_counterTerroristsScoreWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_terroristsScoreWidget{nullptr};

        moe::Ref<moe::BoxWidget> m_remainingPlayersSet{nullptr};
        moe::Ref<moe::VkTextWidget> m_remainingPlayersTextWidget{nullptr};

        void registerDebugCommands(GameManager& ctx);

        void updateBombIconTransparency(float deltaTime);
        void updateCountDownTextWidget(float deltaTime);
    };
}// namespace game::State