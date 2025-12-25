#include "UI/Vulkan/VkImageWidget.hpp"

MOE_BEGIN_NAMESPACE

void VkImageWidget::render(VulkanRenderObjectBus& renderer) {
    if (m_imageId == NULL_IMAGE_ID) {
        return;
    }

    auto bounds = this->calculatedBounds();
    renderer.submitSpriteRender(
            m_imageId,
            Transform{}
                    .setPosition({bounds.x, bounds.y, 0}),
            m_tintColor,
            {bounds.width, bounds.height});
}

MOE_END_NAMESPACE