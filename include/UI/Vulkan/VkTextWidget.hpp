#pragma once

#include "UI/TextWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"


MOE_BEGIN_NAMESPACE

struct VkTextWidget : public TextWidget, public VkWidget {
public:
    VkTextWidget(U32StringView text = U"", FontId fontId = NULL_FONT_ID, float fontSize = 16.f, const Color& style = Colors::White)
        : TextWidget(text, fontSize, style), m_fontId(fontId) {}

    ~VkTextWidget() override = default;

    void render(VulkanRenderObjectBus& renderer) override;

    void setFontId(FontId fontId) {
        m_fontId = fontId;
    }

protected:
    FontId m_fontId{NULL_FONT_ID};
};

MOE_END_NAMESPACE