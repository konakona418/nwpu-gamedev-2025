#include "State/PauseUIState.hpp"
#include "GameManager.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"

namespace game::State {
    void PauseUIState::onEnter(GameManager& ctx) {
        m_fontId = ctx.renderer().getResourceLoader().load(moe::Loader::Font, moe::asset("assets/fonts/NotoSansSC-Regular.ttf"), 48.0f, "");
        // setup UI
        auto [width, height] = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::makeRef<moe::RootWidget>((float) width, (float) height);

        m_titleTextWidget = moe::makeRef<moe::VkTextWidget>(
                U"Paused",
                m_fontId,
                48.f,
                moe::Colors::White);
        m_titleTextWidget->setMargin({0.f, 0.f, 50.f, 0.f});

        m_resumeButtonWidget = moe::makeRef<moe::VkButtonWidget>(
                moe::VkButtonWidget::TextPref{
                        U"Resume",
                        m_fontId,
                        24.f,
                        moe::Colors::Blue,
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
    }

    void PauseUIState::onExit(GameManager& ctx) {
        // cleanup UI
        m_rootWidget = moe::Ref<moe::RootWidget>(nullptr);
        m_titleTextWidget = moe::Ref<moe::VkTextWidget>(nullptr);
        m_resumeButtonWidget = moe::Ref<moe::VkButtonWidget>(nullptr);
    }

    void PauseUIState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_titleTextWidget->render(renderer);
        m_resumeButtonWidget->render(renderer);

        // todo: handle button clicks
    }
}// namespace game::State