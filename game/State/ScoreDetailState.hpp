#pragma once

#include "GameState.hpp"

#include "State/GamePlayData.hpp"

#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkBoxWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "Util.hpp"

#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

namespace game::State {
    class ScoreDetailState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "ScoreDetailState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

    private:
        using FontLoaderT = moe::Secure<game::AnyCacheLoader<game::FontLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;

        struct TempPlayerScoreEntry {
            moe::U32String playerName;
            moe::Color nameColor;
            uint16_t kills;
            uint16_t deaths;
        };

        FontLoaderT m_fontLoader{
                FontLoaderParam{24.0f, Util::glyphRangeChinese()},
                moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf")),
        };
        moe::FontId m_fontId{moe::NULL_FONT_ID};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_baseContainer{nullptr};//vertical - holds title and score list
        moe::Ref<moe::VkTextWidget> m_titleTextWidget{nullptr};
        moe::Ref<moe::BoxWidget> m_scoreListContainer{nullptr};//horizontal - holds player score entries
        moe::Ref<moe::BoxWidget> m_playerNameColumn{nullptr};  //vertical
        moe::Ref<moe::BoxWidget> m_killsColumn{nullptr};       //vertical
        moe::Ref<moe::BoxWidget> m_deathsColumn{nullptr};      //vertical

        moe::Vector<moe::Ref<moe::VkTextWidget>> m_entryWidgets;// all entry text widgets

        void initWidgets(GameManager& ctx);
        void populateScoreEntries(GameManager& ctx);
        void populateScoreEntriesFrom(GameManager& ctx, const moe::Vector<TempPlayerScoreEntry>& entries);

        void registerDebugCommand(GameManager& ctx);

        static moe::Vector<TempPlayerScoreEntry> generateScoreEntriesFromSharedData(moe::Ref<GamePlaySharedData> sharedData);
        static moe::Vector<TempPlayerScoreEntry> generateScoreEntriesMock();
    };
}// namespace game::State