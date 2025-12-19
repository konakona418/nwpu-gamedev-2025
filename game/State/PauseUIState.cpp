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

        m_inputLockToken = 100;
        auto input = ctx.input(m_inputLockToken);// lock input for this state
        if (!input.isValid()) {
            moe::Logger::error("PauseUIState::onEnter: failed to lock input");
            return;
        }

        input->setMouseState(true);// enable mouse cursor
    }

    void PauseUIState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting PauseUIState");

        // release input lock
        auto input = ctx.input(m_inputLockToken);
        if (!input.isValid()) {
            moe::Logger::error("PauseUIState::onExit: input is locked by another state");
            return;
        }

        input->setMouseState(false);// disable mouse cursor
        ctx.unlockInput(m_inputLockToken);
        m_inputLockToken = NO_LOCK_TOKEN;

        // cleanup UI
        m_rootWidget = moe::Ref<moe::RootWidget>(nullptr);
        m_titleTextWidget = moe::Ref<moe::VkTextWidget>(nullptr);
        m_resumeButtonWidget = moe::Ref<moe::VkButtonWidget>(nullptr);
    }

    void PauseUIState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_titleTextWidget->render(renderer);
        m_resumeButtonWidget->render(renderer);

        auto input = ctx.input(m_inputLockToken);
        if (!input.isValid()) {
            moe::Logger::error("PauseUIState::onUpdate: input is locked by another state");
            return;
        }

        auto mousePos = input->getMousePosition();
        bool isLMBPressed = input->getMouseButtonState(0).pressedLMB;

        bool clicked = m_resumeButtonWidget->checkButtonState(glm::vec2(mousePos.first, mousePos.second), isLMBPressed);
        if (clicked) {
            ctx.popState();
        }
    }
}// namespace game::State