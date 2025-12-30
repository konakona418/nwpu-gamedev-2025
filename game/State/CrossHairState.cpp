#include "CrossHairState.hpp"

#include "Param.hpp"

namespace game::State {
    static ParamF4 CROSSHAIR_COLOR("crosshair.color", {1.0f, 1.0f, 1.0f, 1.0f}, ParamScope::UserConfig);
    static ParamI CROSSHAIR_SIZE("crosshair.size", 64, ParamScope::UserConfig);

    void CrossHairState::onEnter(GameManager& ctx) {
        m_crossHairNormalImageId = m_crossHairImageLoader.generate().value_or(moe::NULL_IMAGE_ID);
        if (m_crossHairNormalImageId == moe::NULL_IMAGE_ID) {
            moe::Logger::error("CrossHairState::onEnter: failed to load crosshair image");
            return;
        }

        m_crossHairShotImageId = m_crossHairShotImageLoader.generate().value_or(moe::NULL_IMAGE_ID);
        if (m_crossHairShotImageId == moe::NULL_IMAGE_ID) {
            moe::Logger::error("CrossHairState::onEnter: failed to load crosshair shot image");
            return;
        }

        auto canvasSize = ctx.renderer().getCanvasSize();
        m_rootWidget = moe::Ref(new moe::RootWidget(canvasSize.first, canvasSize.second));

        auto container = moe::makeRef<moe::BoxWidget>(moe::BoxLayoutDirection::Vertical);
        container->setAlign(moe::BoxAlign::Center);
        container->setJustify(moe::BoxJustify::Center);

        // init color
        auto color_ = CROSSHAIR_COLOR.get();
        moe::Color crosshairColor = {color_.x, color_.y, color_.z, color_.w};
        m_crossHairImageWidget = moe::Ref(
                new moe::VkImageWidget(m_crossHairNormalImageId,
                                       moe::LayoutSize{
                                               (float) CROSSHAIR_SIZE.get(),
                                               (float) CROSSHAIR_SIZE.get(),
                                       },
                                       crosshairColor));
        container->addChild(m_crossHairImageWidget);

        m_rootWidget->addChild(container);

        m_rootWidget->layout();
    }

    void CrossHairState::onExit(GameManager& ctx) {
        m_rootWidget.reset();
        m_crossHairImageWidget.reset();
    }

    void CrossHairState::onUpdate(GameManager& ctx, float deltaTime) {
        auto& renderer = ctx.renderer().getBus<moe::VulkanRenderObjectBus>();

        // update crosshair color
        auto color_ = CROSSHAIR_COLOR.get();
        moe::Color crosshairColor = {color_.x, color_.y, color_.z, color_.w};
        m_crossHairImageWidget->setTintColor(crosshairColor);

        m_crossHairImageWidget->render(renderer);
    }
}// namespace game::State