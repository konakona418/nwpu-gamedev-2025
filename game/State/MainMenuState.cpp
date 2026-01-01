#include "MainMenuState.hpp"

#include "App.hpp"
#include "GameManager.hpp"
#include "Localization.hpp"
#include "Param.hpp"

#include "UI/BoxWidget.hpp"

#include "State/CreditsState.hpp"
#include "State/GamePlayState.hpp"

namespace game::State {
    static I18N MAINMENU_TITLE("mainmenu.title", U"Game Title");

    static I18N MAINMENU_MULTIPLAYER_BUTTON("mainmenu.multiplayer_button", U"Multiplayer");
    static I18N MAINMENU_SETTINGS_BUTTON("mainmenu.settings_button", U"Settings");
    static I18N MAINMENU_CREDITS_BUTTON("mainmenu.credits_button", U"Credits");
    static I18N MAINMENU_EXIT_BUTTON("mainmenu.exit_button", U"Exit");

    static ParamF4 MAINMENU_CAMERA_POSITION(
            "mainmenu.camera.position",
            ParamFloat4{
                    23.0f,
                    -2.0f,
                    36.0f,
                    0.0f,
            });
    static ParamF MAINMENU_CAMERA_YAW_DEGREES("mainmenu.camera.yaw_degrees", -50.0f);
    static ParamF MAINMENU_CAMERA_PITCH_DEGREES("mainmenu.camera.pitch_degrees", 0.0f);

    static ParamF4 MAINMENU_COUNTERTERRORIST_POSITION(
            "mainmenu.counter_terrorist.position",
            ParamFloat4{
                    28.0f,
                    -4.0f,
                    30.0f,
                    0.0f,
            });
    static ParamF MAINMENU_COUNTERTERRORIST_PITCH_DEGREES("mainmenu.counter_terrorist.pitch_degrees", 0.0f);

    void MainMenuState::initModels(GameManager& ctx) {
        auto playgroundModel = m_playgroundModelLoader.generate();
        if (!playgroundModel) {
            moe::Logger::error("Failed to load playground model in MainMenuState");
            return;
        }
        m_playgroundRenderable = playgroundModel.value();

        auto ctModel = m_counterTerroristModelLoader.generate();
        if (!ctModel) {
            moe::Logger::error("Failed to load counter-terrorist model in MainMenuState");
            return;
        }
        m_counterTerroristRenderable = ctModel.value();

        auto ctScene = ctx.renderer().m_caches.objectCache.get(m_counterTerroristRenderable).value();
        auto* animatableRenderable = ctScene->checkedAs<moe::VulkanSkeletalAnimation>(moe::VulkanRenderableFeature::HasSkeletalAnimation).value();
        m_counterTerroristIdleAnimation = animatableRenderable->getAnimations().at("Idle");
    }

    void MainMenuState::onEnter(GameManager& ctx) {
        moe::Logger::info("Entering MainMenuState");

        auto& input = ctx.input();
        input.addProxy(&m_inputProxy);

        m_inputProxy.setMouseState(true);

        initModels(ctx);

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
        auto& renderCamera = ctx.renderer().getDefaultCamera();

        m_titleTextWidget->render(renderctx);

        auto mousePos = m_inputProxy.getMousePosition();
        bool isLMBPressed = m_inputProxy.getMouseButtonState().pressedLMB;

        // force 3d camera position
        auto cameraPos = MAINMENU_CAMERA_POSITION.get();
        renderCamera.setPosition(
                glm::vec3{
                        cameraPos.x,
                        cameraPos.y,
                        cameraPos.z,
                });
        renderCamera.setYaw(MAINMENU_CAMERA_YAW_DEGREES.get());
        renderCamera.setPitch(MAINMENU_CAMERA_PITCH_DEGREES.get());

        // render playground model
        renderctx.submitRender(
                m_playgroundRenderable,
                moe::Transform{});

        // update and render counter-terrorist model
        m_counterTerroristIdleAnimationProgressSecs += deltaTime;
        if (m_counterTerroristIdleAnimationProgressSecs >= c_counterTerroristIdleAnimationDurationSecs) {
            m_counterTerroristIdleAnimationProgressSecs = 0;
        }
        auto computeHandle = renderctx.submitComputeSkin(
                m_counterTerroristRenderable,
                m_counterTerroristIdleAnimation,
                m_counterTerroristIdleAnimationProgressSecs);
        auto ctPos = MAINMENU_COUNTERTERRORIST_POSITION.get();
        renderctx.submitRender(
                m_counterTerroristRenderable,
                moe::Transform{}
                        .setPosition({ctPos.x, ctPos.y, ctPos.z})
                        .setRotation(glm::vec3{
                                0.0f,
                                glm::radians(MAINMENU_CAMERA_PITCH_DEGREES.get()),
                                0.0f,
                        }),
                computeHandle);

        // render ui
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

            auto creditsState = moe::Ref(new CreditsState());
            this->addChildState(creditsState);
        }

        m_exitButtonWidget->render(renderctx);
        if (m_exitButtonWidget->checkButtonState(mousePos, isLMBPressed)) {
            moe::Logger::info("Exit button clicked");
            ctx.app().requestExit();
        }
    }

    void MainMenuState::onStateChanged(GameManager& ctx, bool isTopmostState) {
        moe::Logger::info("MainMenuState::onStateChanged: isTopmostState = {}", isTopmostState);
        m_inputProxy.setActive(isTopmostState);
    }
}// namespace game::State