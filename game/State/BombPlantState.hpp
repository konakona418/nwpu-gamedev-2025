#pragma once

#include "GameState.hpp"

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
    struct BombPlantState : public GameState {
    public:
        const moe::StringView getName() const override {
            return "BombPlantState";
        }

        BombPlantState() = default;

        void onEnter(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;
        void onExit(GameManager& ctx) override;

        void setVisible(bool isVisible) {
            m_isVisible = isVisible;
        }

        bool isVisible() const {
            return m_isVisible;
        }

        void setPlantingProgress(float progress);

        float getPlantingProgress() const {
            return m_plantingProgress;
        }

    private:
        using FontLoaderT =
                moe::Secure<game::AnyCacheLoader<game::FontLoader<
                        moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;

        FontLoaderT m_fontLoader{
                FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf")),
        };
        moe::FontId m_fontId{moe::NULL_FONT_ID};

        moe::Ref<moe::RootWidget> m_rootWidget;
        moe::Ref<moe::VkBoxWidget> m_containerWidget;
        moe::Ref<moe::VkTextWidget> m_plantingTextWidget;

        bool m_isVisible{false};
        float m_plantingProgress{0.0f};
    };
}// namespace game::State