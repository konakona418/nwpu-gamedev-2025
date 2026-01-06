#include "State/SplashScreenState.hpp"

#include "GameManager.hpp"
#include "Localization.hpp"
#include "ObjectPool.hpp"
#include "Param.hpp"

#include "State/MainMenuState.hpp"

namespace game::State {
    static ParamF SPLASHSCREEN_DURATION(
            "splashscreen.duration",
            0.75f, ParamScope::System);

    static I18N SPLASH_LOADING_TEXT(
            "splashscreen.loading_text",
            U"Preloading Objects...{} / {}");

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
        m_logoImageWidget->setMargin({0, 0, 0, 20});

        m_loadingTextWidget = moe::Ref(
                new moe::VkTextWidget(
                        U"",
                        m_fontLoader.generate().value_or(moe::NULL_FONT_ID),
                        18.0f,
                        moe::Colors::White));
        m_loadingTextWidget->setMargin({0, 0, 0, 16});

        m_progressBarWidget = moe::Ref(
                new moe::VkProgressBarWidget(
                        {400.f, 12.f},
                        moe::Color::fromNormalized(50, 50, 50, 255),
                        moe::Colors::White));

        m_containerWidget->addChild(m_logoImageWidget);
        m_containerWidget->addChild(m_loadingTextWidget);
        m_containerWidget->addChild(m_progressBarWidget);

        rootWidget->addChild(m_containerWidget);

        rootWidget->layout();
    }

    void SplashScreenState::onExit(GameManager& ctx) {
        moe::Logger::info("Exiting SplashScreenState");
    }

    void SplashScreenState::onUpdate(GameManager& ctx, float deltaTime) {
        m_elapsedTimeSecs += deltaTime;
        float duration = SPLASHSCREEN_DURATION.get();

        float progress = 0.0f;

        auto& objectPool = ObjectPool::getInstance();
        switch (m_currentPhase) {
            case SplashPhase::FadeIn: {
                if (m_elapsedTimeSecs >= duration) {
                    m_currentPhase = SplashPhase::Display;
                    m_elapsedTimeSecs = 0.0f;
                    break;
                }

                float fadeFactor = m_fadeInTween.eval(m_elapsedTimeSecs / duration);
                m_logoImageWidget->setTintColor(
                        moe::Color::fromNormalized(
                                255,
                                255,
                                255,
                                static_cast<uint8_t>(fadeFactor * 255)));
                break;
            }
            case SplashPhase::Display: {
                m_logoImageWidget->setTintColor(
                        moe::Color::fromNormalized(
                                255,
                                255,
                                255,
                                255));

                size_t pendingLoads = objectPool.getPendingLoadCount();
                size_t totalLoads = objectPool.getPoolSize();

                progress = totalLoads == 0 ? 1.0f : static_cast<float>(totalLoads - pendingLoads) / static_cast<float>(totalLoads);
                m_progressBarWidget->setProgress(progress);

                if (totalLoads != m_totalObjectsToLoad || totalLoads - pendingLoads != m_loadedObjectsCount) {
                    m_totalObjectsToLoad = totalLoads;
                    m_loadedObjectsCount = totalLoads - pendingLoads;

                    m_loadingTextWidget->setText(
                            Util::formatU32(
                                    SPLASH_LOADING_TEXT.get(),
                                    m_loadedObjectsCount,
                                    m_totalObjectsToLoad));
                    m_rootWidget->layout();
                }

                bool isComplete = !objectPool.loadOneRegisteredRenderable(ctx);
                if (isComplete) {
                    m_currentPhase = SplashPhase::FadeOut;
                    m_elapsedTimeSecs = 0.0f;
                }

                break;
            }
            case SplashPhase::FadeOut: {
                if (m_elapsedTimeSecs >= duration) {
                    ctx.queueFree(this->intoRef());

                    auto mainMenuState = moe::Ref(new MainMenuState());
                    ctx.pushState(mainMenuState);// transition to main menu
                    return;
                }

                float fadeFactor = m_fadeOutTween.eval(m_elapsedTimeSecs / duration);
                m_logoImageWidget->setTintColor(
                        moe::Color::fromNormalized(
                                255,
                                255,
                                255,
                                static_cast<uint8_t>(fadeFactor * 255)));
                break;
            }
        }

        auto& renderctx = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();
        m_containerWidget->render(renderctx);
        m_logoImageWidget->render(renderctx);

        if (m_currentPhase == SplashPhase::Display) {
            m_loadingTextWidget->render(renderctx);
            m_progressBarWidget->render(renderctx);
        }
    }
}// namespace game::State