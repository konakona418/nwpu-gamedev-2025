#include "State/PauseUIState.hpp"

#include "App.hpp"
#include "GameManager.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"

namespace game::State {
    void PauseUIState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering PauseUIState");

        auto fontId = this->m_fontId.generate().value_or(moe::NULL_FONT_ID);
        if (fontId == moe::NULL_FONT_ID) {
            moe::Logger::error("PauseUIState::onEnter: failed to load font");
            return;
        }

        // setup UI
        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>((float) width, (float) height);

        m_titleTextWidget = moe::makeRef<moe::VkTextWidget>(
                U"Paused",
                fontId,
                48.f,
                moe::Colors::White);
        m_titleTextWidget->setMargin({0.f, 0.f, 50.f, 0.f});

        m_resumeButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        U"Resume",
                        fontId,
                        24.f,
                        moe::Color::fromNormalized(50, 50, 50, 255),
                },
                moe::VkButtonWidget::ButtonPref{
                        .preferredSize = {200.f, 50.f},
                });
        m_resumeButtonWidget->setMargin({0.f, 0.f, 20.f, 0.f});

        auto container = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
        container->addChild(m_titleTextWidget);
        container->addChild(m_resumeButtonWidget);
        container->setPadding({20.f, 20.f, 20.f, 20.f});
        container->setMargin({(width - 200.f) / 2.f, 0.f, (height - 150.f) / 2.f, 0.f});
        m_rootWidget->addChild(container);

        m_rootWidget->layout();

        auto& input = ctx.input();
        input.addProxy(&m_inputProxy);

        m_inputProxy.setMouseState(true);// enable mouse cursor
    }

    void PauseUIState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting PauseUIState");

        auto& input = ctx.input();
        m_inputProxy.setMouseState(false);// disable mouse cursor
        // release input proxy
        input.removeProxy(&m_inputProxy);

        // cleanup UI
        m_rootWidget = moe::Ref<moe::RootWidget>(nullptr);
        m_titleTextWidget = moe::Ref<moe::VkTextWidget>(nullptr);
        m_resumeButtonWidget = moe::Ref<moe::VkButtonWidget>(nullptr);
    }

    void PauseUIState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_titleTextWidget->render(renderer);
        m_resumeButtonWidget->render(renderer);

        auto& input = ctx.input();
        if (!m_inputProxy.isValid()) {
            return;
        }

        auto mousePos = m_inputProxy.getMousePosition();
        bool isLMBPressed = m_inputProxy.getMouseButtonState(0).pressedLMB;

        bool clicked = m_resumeButtonWidget->checkButtonState(glm::vec2(mousePos.first, mousePos.second), isLMBPressed);
        if (clicked) {
            ctx.popState();
        }
    }
}// namespace game::State