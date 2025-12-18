#pragma once

#include "UI/SpriteWidget.hpp"
#include "UI/Vulkan/VkTextWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"

MOE_BEGIN_NAMESPACE

struct VkButtonWidget : public SpriteWidget, public VkWidget {
public:
    struct TextPref {
        U32StringView text;
        FontId fontId{NULL_FONT_ID};
        float fontSize{16.f};
        Color color{Colors::White};
    };

    struct ButtonPref {
        LayoutSize preferredSize{128.f, 48.f};

        Color defaultBackgroundColor = Color::fromNormalized(200, 200, 200, 255);
        ImageId defaultBackgroundImage{NULL_IMAGE_ID};
        Color hoveredBackgroundColor = Color::fromNormalized(220, 220, 220, 255);
        ImageId hoveredBackgroundImage{NULL_IMAGE_ID};
        Color pressedBackgroundColor = Color::fromNormalized(180, 180, 180, 255);
        ImageId pressedBackgroundImage{NULL_IMAGE_ID};
    };

    VkButtonWidget(TextPref textPref, ButtonPref buttonPref)
        : SpriteWidget(buttonPref.preferredSize),
          m_textWidget(Ref<VkTextWidget>(new VkTextWidget(textPref.text, textPref.fontId, textPref.fontSize, textPref.color))),
          m_buttonPref(buttonPref) {
        this->addChild(m_textWidget);
    }

    ~VkButtonWidget() override = default;

    void layout(const LayoutRect& rectAssigned) override;

    void render(VulkanRenderObjectBus& renderer) override;

    bool checkButtonState(const glm::vec2& cursorPos, bool isPressed);

    void setButtonPref(const ButtonPref& buttonPref) {
        m_buttonPref = buttonPref;
    }

    void setTextPref(const TextPref& textPref) {
        m_textWidget->setText(textPref.text);
        m_textWidget->setFontSize(textPref.fontSize);
        m_textWidget->setColor(textPref.color);
        m_textWidget->setFontId(textPref.fontId);
    }

protected:
    Ref<VkTextWidget> m_textWidget;
    ButtonPref m_buttonPref;

    enum class ButtonState {
        Normal,
        Hovered,
        Pressed
    } m_buttonState{ButtonState::Normal};

    bool m_wasPressedLastFrame{false};
};

MOE_END_NAMESPACE