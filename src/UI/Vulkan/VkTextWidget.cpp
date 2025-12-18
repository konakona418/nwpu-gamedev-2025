#include "UI/Vulkan/VkTextWidget.hpp"

MOE_BEGIN_NAMESPACE

void VkTextWidget::render(VulkanRenderObjectBus& renderer) {
    if (m_fontId == NULL_FONT_ID) {
        Logger::warn("VkTextWidget render called with NULL_FONT_ID");
        return;
    }

    const auto& bounds = this->calculatedBounds();

    for (const auto& line: this->m_lines) {
        renderer.submitTextSpriteRender(
                m_fontId,
                this->m_fontSize,
                line.text,
                Transform{}.setPosition(glm::vec3{bounds.x, bounds.y + line.yOffset, 0.0f}),
                this->m_color);
    }
}

MOE_END_NAMESPACE