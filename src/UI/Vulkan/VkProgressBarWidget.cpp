#include "UI/Vulkan/VkProgressBarWidget.hpp"

MOE_BEGIN_NAMESPACE

void VkProgressBarWidget::render(VulkanRenderObjectBus& renderer) {
    auto& rect = m_spriteRenderRect;

    // render background
    renderer.submitSpriteRender(
            NULL_IMAGE_ID,
            Transform{}.setPosition({rect.x, rect.y, 0}),
            backgroundColor,
            {rect.width, rect.height});

    // render fill
    float fillWidth = rect.width * m_progress;
    renderer.submitSpriteRender(
            NULL_IMAGE_ID,
            Transform{}.setPosition({rect.x, rect.y, 0}),
            fillColor,
            {fillWidth, rect.height});
}

MOE_END_NAMESPACE