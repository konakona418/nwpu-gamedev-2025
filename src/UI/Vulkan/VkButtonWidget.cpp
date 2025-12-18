#include "UI/Vulkan/VkButtonWidget.hpp"

MOE_BEGIN_NAMESPACE

void VkButtonWidget::layout(const LayoutRect& rectAssigned) {
    SpriteWidget::layout(rectAssigned);

    // put text widget in center
    const auto& bounds = this->calculatedBounds();
    const auto textPreferredSize = m_textWidget->preferredSize(
            LayoutConstraints{
                    0,
                    bounds.width,
                    0,
                    bounds.height,
            });

    float textX = bounds.x + (bounds.width - textPreferredSize.width) / 2.0f;
    float textY = bounds.y + (bounds.height - textPreferredSize.height) / 2.0f;

    m_textWidget->layout(LayoutRect{
            textX,
            textY,
            textPreferredSize.width,
            textPreferredSize.height,
    });
}

void VkButtonWidget::render(VulkanRenderObjectBus& renderer) {
    const auto& bounds = this->calculatedBounds();

    const auto transform = Transform{}.setPosition(glm::vec3{bounds.x, bounds.y, 0.0f});
    const auto size = glm::vec2{bounds.width, bounds.height};

    switch (m_buttonState) {
        case ButtonState::Normal:
            renderer.submitSpriteRender(
                    m_buttonPref.defaultBackgroundImage,
                    transform,
                    m_buttonPref.defaultBackgroundColor,
                    size);
            break;
        case ButtonState::Hovered:
            renderer.submitSpriteRender(
                    m_buttonPref.hoveredBackgroundImage,
                    transform,
                    m_buttonPref.hoveredBackgroundColor,
                    size);
            break;
        case ButtonState::Pressed:
            renderer.submitSpriteRender(
                    m_buttonPref.pressedBackgroundImage,
                    transform,
                    m_buttonPref.pressedBackgroundColor,
                    size);
            break;
    }

    m_textWidget->render(renderer);
}

void VkButtonWidget::checkButtonState(const glm::vec2& cursorPos, bool isPressed) {
    const auto& bounds = this->calculatedBounds();
    bool isHovered = cursorPos.x >= bounds.x &&
                     cursorPos.x <= bounds.x + bounds.width &&
                     cursorPos.y >= bounds.y &&
                     cursorPos.y <= bounds.y + bounds.height;

    if (isHovered) {
        if (isPressed) {
            m_buttonState = ButtonState::Pressed;
        } else {
            m_buttonState = ButtonState::Hovered;
        }
    } else {
        m_buttonState = ButtonState::Normal;
    }
}

MOE_END_NAMESPACE