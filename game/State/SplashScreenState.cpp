#include "State/SplashScreenState.hpp"

#include "GameManager.hpp"
#include "Param.hpp"

#include "State/MainMenuState.hpp"

namespace game::State {
    static ParamF SPLASHSCREEN_DURATION(
            "splashscreen.duration",
            3.0f, ParamScope::System);

    void SplashScreenState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering SplashScreenState");

        m_elapsedTimeSecs = 0.0f;

        m_logoImageId = ctx.renderer().getResourceLoader().load(moe::Loader::Image, moe::asset("assets/images/logo.png"));

        auto [width, height] = ctx.renderer().getCanvasSize();
        auto rootWidget = moe::Ref(new moe::RootWidget(width, height));
        m_rootWidget = rootWidget;

        m_containerWidget = moe::Ref(new moe::VkBoxWidget(moe::BoxLayoutDirection::Vertical));
        m_containerWidget->setAlign(moe::BoxAlign::Center);
        m_containerWidget->setJustify(moe::BoxJustify::Center);
        m_containerWidget->setBackgroundColor(moe::Color::fromNormalized(0, 0, 0, 255));

        m_logoImageWidget = moe::Ref(new moe::VkImageWidget(m_logoImageId, {600, 240}, moe::Colors::Transparent));

        m_containerWidget->addChild(m_logoImageWidget);

        rootWidget->addChild(m_containerWidget);

        rootWidget->layout();
    }

    void SplashScreenState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting SplashScreenState");
    }

    void SplashScreenState::onUpdate(GameManager& ctx, float deltaTime) {
        m_elapsedTimeSecs += deltaTime;
        float duration = SPLASHSCREEN_DURATION.get();
        if (m_elapsedTimeSecs >= duration) {
            ctx.queueFree(this->intoRef());

            auto mainMenuState = moe::Ref(new MainMenuState());
            ctx.pushState(mainMenuState);// transition to main menu
            return;
        }

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_containerWidget->render(renderctx);
        m_logoImageWidget->render(renderctx);

        float fadeFactor = m_fadeTween.eval(m_elapsedTimeSecs / duration);
        m_logoImageWidget->setTintColor(
                moe::Color::fromNormalized(
                        255,
                        255,
                        255,
                        static_cast<uint8_t>(fadeFactor * 255)));
    }
}// namespace game::State