#include "UI/Vulkan/VkBoxWidget.hpp"

MOE_BEGIN_NAMESPACE

void VkBoxWidget::render(VulkanRenderObjectBus& renderer) {
    auto& bounds = calculatedBounds();
    renderer.submitSpriteRender(
            NULL_IMAGE_ID,
            Transform{}.setPosition({bounds.x, bounds.y, 0}),
            m_backgroundColor,
            {bounds.width, bounds.height});
}

MOE_END_NAMESPACE