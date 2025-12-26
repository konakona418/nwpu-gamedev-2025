#include "State/PurchaseState.hpp"

#include "Localization.hpp"

namespace game::State {
    const moe::Pair<moe::String, PurchaseState::Items> PurchaseState::s_itemNameToEnumMap[] = {
            {"Glock", PurchaseState::Items::Glock},
            {"USP", PurchaseState::Items::USP},
            {"Desert Eagle", PurchaseState::Items::DesertEagle},
            {"AK47", PurchaseState::Items::AK47},
            {"M4A1", PurchaseState::Items::M4A1},
    };

    static I18N PURCHASE_MENU_TITLE("purchase_menu.title", U"Purchase Phase");

    void PurchaseState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering PurchaseState");

        auto fontId = m_fontLoader.generate().value_or(moe::NULL_FONT_ID);
        if (fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("PurchaseState::onEnter: failed to load font");
            return;
        }
        m_fontId = fontId;

        // setup UI
        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>((float) width, (float) height);

        m_containerWidget =
                moe::Ref(new moe::VkBoxWidget(moe::BoxLayoutDirection::Vertical));
        m_containerWidget->setBackgroundColor(moe::Color{0.f, 0.f, 0.f, 0.75f});
        m_containerWidget->setPadding({20.f, 20.f, 20.f, 20.f});

        m_containerWidget->setAlign(moe::BoxAlign::Center);
        m_containerWidget->setJustify(moe::BoxJustify::Center);

        m_titleTextWidget = moe::makeRef<moe::VkTextWidget>(
                PURCHASE_MENU_TITLE.get(),
                m_fontId,
                36.f,
                moe::Colors::White);
        m_titleTextWidget->setMargin({10.f, 0.f, 10.f, 20.f});
        m_containerWidget->addChild(m_titleTextWidget);

        for (const auto& [itemName, itemEnum]: s_itemNameToEnumMap) {
            auto itemButton = moe::makeRef<moe::VkButtonWidget>(
                    moe::VkButtonWidget::TextPref{
                            utf8::utf8to32(itemName),
                            m_fontId,
                            24.f,
                            moe::Color::fromNormalized(50, 50, 50, 255),
                    },
                    moe::VkButtonWidget::ButtonPref{
                            .preferredSize = {200.f, 50.f},
                    });
            itemButton->setMargin({10.f, 10.f, 10.f, 10.f});

            m_containerWidget->addChild(itemButton);
            m_itemButtonWidgets.push_back(itemButton);
        }

        m_rootWidget->addChild(m_containerWidget);

        m_rootWidget->layout();

        auto& input = ctx.input();
        input.addProxy(&m_inputProxy);
        m_inputProxy.setMouseState(true);// enable mouse cursor
    }

    void PurchaseState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting PurchaseState");

        auto& input = ctx.input();
        m_inputProxy.setMouseState(false);// disable mouse cursor
        input.removeProxy(&m_inputProxy);

        m_rootWidget.reset();
        m_itemButtonWidgets.clear();
    }

    void PurchaseState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_containerWidget->render(renderer);
        m_titleTextWidget->render(renderer);

        for (auto& itemButton: m_itemButtonWidgets) {
            itemButton->render(renderer);
            auto cursorPos = m_inputProxy.getMousePosition();
            auto lmbPressed = m_inputProxy.getMouseButtonState().pressedLMB;
            if (itemButton->checkButtonState(cursorPos, lmbPressed)) {
                moe::Logger::info("Item button '{}' clicked", utf8::utf32to8(itemButton->text()));
                // todo: send purchase request to server
            }
        }
    }
}// namespace game::State