#include "MainMenuState.hpp"

#include "GameManager.hpp"
#include "UI/BoxWidget.hpp"

namespace game::State {
    void MainMenuState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering MainMenuState");

        auto& input = ctx.input();
        input.addProxy(&m_inputProxy);

        m_inputProxy.setMouseState(true);

        auto canvas = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(canvas.first, canvas.second);

        auto boxWidget = moe::makeRef<moe::BoxWidget>(
                moe::BoxLayoutDirection::Vertical);
        boxWidget->setJustify(moe::BoxJustify::SpaceBetween);
        boxWidget->setAlign(moe::BoxAlign::Start);
        boxWidget->setPadding({20.f, 20.f, 20.f, 20.f});
        boxWidget->setMargin({(canvas.first - 200.f) / 2.f, 0.f, (canvas.second - 300.f) / 2.f, 0.f});
        m_rootWidget->addChild(boxWidget);

        m_multiPlayerButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        U"Multiplayer",
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });

        m_settingsButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        U"Settings",
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });

        m_creditsButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        U"Credits",
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });

        m_exitButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        U"Exit",
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });

        boxWidget->addChild(m_multiPlayerButtonWidget);
        boxWidget->addChild(m_settingsButtonWidget);
        boxWidget->addChild(m_creditsButtonWidget);
        boxWidget->addChild(m_exitButtonWidget);

        m_rootWidget->layout();
    }

    void MainMenuState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting MainMenuState");

        m_rootWidget = moe::Ref<moe::RootWidget>::null();
        m_multiPlayerButtonWidget = moe::Ref<moe::VkButtonWidget>::null();
        m_settingsButtonWidget = moe::Ref<moe::VkButtonWidget>::null();
        m_creditsButtonWidget = moe::Ref<moe::VkButtonWidget>::null();
        m_exitButtonWidget = moe::Ref<moe::VkButtonWidget>::null();

        m_inputProxy.setMouseState(false);

        auto& input = ctx.input();
        input.removeProxy(&m_inputProxy);
    }

    void MainMenuState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        auto mousePos_ = m_inputProxy.getMousePosition();
        auto mousePos = glm::vec2(mousePos_.first, mousePos_.second);
        bool isLMBPressed = m_inputProxy.getMouseButtonState(0).pressedLMB;

        m_multiPlayerButtonWidget->render(renderctx);
        if (m_multiPlayerButtonWidget->checkButtonState(mousePos, isLMBPressed)) {
            moe::Logger::info("Multiplayer button clicked");
        }

        m_settingsButtonWidget->render(renderctx);
        if (m_settingsButtonWidget->checkButtonState(mousePos, isLMBPressed)) {
            moe::Logger::info("Settings button clicked");
        }

        m_creditsButtonWidget->render(renderctx);
        if (m_creditsButtonWidget->checkButtonState(mousePos, isLMBPressed)) {
            moe::Logger::info("Credits button clicked");
        }

        m_exitButtonWidget->render(renderctx);
        if (m_exitButtonWidget->checkButtonState(mousePos, isLMBPressed)) {
            moe::Logger::info("Exit button clicked");
        }
    }
}// namespace game::State