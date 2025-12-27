#pragma once

#include "GameManager.hpp"
#include "GameState.hpp"
#include "Input.hpp"

#include "AnyCache.hpp"
#include "FontLoader.hpp"
#include "Util.hpp"

#include "UI/RootWidget.hpp"
#include "UI/Vulkan/VkBoxWidget.hpp"
#include "UI/Vulkan/VkButtonWidget.hpp"


#include "Core/Resource/BinaryLoader.hpp"
#include "Core/Resource/Launch.hpp"
#include "Core/Resource/Preload.hpp"
#include "Core/Resource/Secure.hpp"

namespace game::State {
    struct PurchaseState : public GameState {
    public:
        enum class Items {
            None,
            Glock,
            USP,
            DesertEagle,
            AK47,
            M4A1,
        };

        const moe::StringView getName() const override {
            return "PurchaseState";
        }

        void onEnter(GameManager& ctx) override;
        void onExit(GameManager& ctx) override;
        void onUpdate(GameManager& ctx, float deltaTime) override;

        static const moe::Pair<moe::String, Items> s_itemNameToEnumMap[];

    private:
        using FontLoaderT = moe::Secure<game::AnyCacheLoader<game::FontLoader<
                moe::Preload<moe::Launch<game::AnyCacheLoader<moe::BinaryLoader>>>>>>;

        FontLoaderT m_fontLoader{
                FontLoaderParam{48.0f, Util::glyphRangeChinese()},
                moe::BinaryFilePath(moe::asset("assets/fonts/NotoSansSC-Regular.ttf"))};
        moe::FontId m_fontId{moe::NULL_FONT_ID};

        moe::Ref<moe::RootWidget> m_rootWidget{nullptr};
        moe::Ref<moe::VkTextWidget> m_titleTextWidget{nullptr};
        moe::Ref<moe::VkBoxWidget> m_containerWidget{nullptr};
        moe::Vector<moe::Ref<moe::VkButtonWidget>> m_itemButtonWidgets;
        moe::Ref<moe::VkTextWidget> m_balanceTextWidget{nullptr};

        uint32_t m_lastKnownBalance{0};

        InputProxy m_inputProxy{InputProxy::PRIORITY_UI_LOCK};
    };
}// namespace game::State