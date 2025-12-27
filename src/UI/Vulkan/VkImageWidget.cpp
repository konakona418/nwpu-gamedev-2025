#include "UI/Vulkan/VkImageWidget.hpp"

MOE_BEGIN_NAMESPACE

void VkImageWidget::render(VulkanRenderObjectBus& renderer) {
    if (m_imageId == NULL_IMAGE_ID) {
        return;
    }

    auto rect = this->m_spriteRenderRect;
    renderer.submitSpriteRender(
            m_imageId,
            Transform{}
                    .setPosition({rect.x, rect.y, 0}),
            m_tintColor,
            {rect.width, rect.height});
}

MOE_END_NAMESPACE