#include "MainMenuState.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"

#include "UI/BoxWidget.hpp"

#include "State/GamePlayState.hpp"

namespace game::State {
    static I18N MAINMENU_TITLE("mainmenu.title", U"Game Title");

    static I18N MAINMENU_MULTIPLAYER_BUTTON("mainmenu.multiplayer_button", U"Multiplayer");
    static I18N MAINMENU_SETTINGS_BUTTON("mainmenu.settings_button", U"Settings");
    static I18N MAINMENU_CREDITS_BUTTON("mainmenu.credits_button", U"Credits");
    static I18N MAINMENU_EXIT_BUTTON("mainmenu.exit_button", U"Exit");

    void MainMenuState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering MainMenuState");

        auto& input = ctx.input();
        input.addProxy(&m_inputProxy);

        m_inputProxy.setMouseState(true);

        auto canvas = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>(canvas.first, canvas.second);

        auto mainBoxWidget =
                moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
        mainBoxWidget->setJustify(moe::BoxJustify::SpaceBetween);

        m_titleTextWidget = moe::makeRef<moe::VkTextWidget>(
                MAINMENU_TITLE.get(),
                m_fontId.generate().value_or(moe::NULL_FONT_ID),
                48.f,
                moe::Color::fromNormalized(255, 255, 255, 255));
        m_titleTextWidget->setMargin({50.f, 50.f, 0.f, 0.f});
        mainBoxWidget->addChild(m_titleTextWidget);

        auto boxWidget = moe::makeRef<moe::BoxWidget>(
                moe::BoxLayoutDirection::Vertical);
        boxWidget->setJustify(moe::BoxJustify::End);
        boxWidget->setAlign(moe::BoxAlign::Start);
        boxWidget->setPadding({40.f, 50.f, 50.f, 50.f});

        m_multiPlayerButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        MAINMENU_MULTIPLAYER_BUTTON.get(),
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });
        m_multiPlayerButtonWidget->setMargin({0.f, 0.f, 0.f, 20.f});

        m_settingsButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        MAINMENU_SETTINGS_BUTTON.get(),
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });
        m_settingsButtonWidget->setMargin({0.f, 0.f, 0.f, 20.f});

        m_creditsButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        MAINMENU_CREDITS_BUTTON.get(),
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });
        m_creditsButtonWidget->setMargin({0.f, 0.f, 0.f, 20.f});

        m_exitButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        MAINMENU_EXIT_BUTTON.get(),
                        m_fontId.generate().value_or(moe::NULL_FONT_ID),
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });
        m_exitButtonWidget->setMargin({0.f, 0.f, 0.f, 20.f});

        boxWidget->addChild(m_multiPlayerButtonWidget);
        boxWidget->addChild(m_settingsButtonWidget);
        boxWidget->addChild(m_creditsButtonWidget);
        boxWidget->addChild(m_exitButtonWidget);

        mainBoxWidget->addChild(boxWidget);

        m_rootWidget->addChild(mainBoxWidget);
        m_rootWidget->layout();
    }

    void MainMenuState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting MainMenuState");

        m_rootWidget.reset();
        m_titleTextWidget.reset();
        m_multiPlayerButtonWidget.reset();
        m_settingsButtonWidget.reset();
        m_creditsButtonWidget.reset();
        m_exitButtonWidget.reset();

        m_inputProxy.setMouseState(false);

        auto& input = ctx.input();
        input.removeProxy(&m_inputProxy);
    }

    void MainMenuState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        m_titleTextWidget->render(renderctx);

        auto mousePos = m_inputProxy.getMousePosition();
        bool isLMBPressed = m_inputProxy.getMouseButtonState().pressedLMB;

        m_multiPlayerButtonWidget->render(renderctx);
        if (m_multiPlayerButtonWidget->checkButtonState(mousePos, isLMBPressed)) {
            moe::Logger::info("Multiplayer button clicked");

            // enter gameplay state
            auto gamePlayState = moe::Ref(new GamePlayState());
            ctx.pushState(gamePlayState);
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

    void MainMenuState::onStateChanged(GameManager& ctx, bool isTopmostState) {
        moe::Logger::info("MainMenuState::onStateChanged: isTopmostState = {}", isTopmostState);
        m_inputProxy.setActive(isTopmostState);
    }
}// namespace game::State