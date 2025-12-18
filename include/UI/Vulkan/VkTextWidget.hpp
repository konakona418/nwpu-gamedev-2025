#pragma once

#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanFont.hpp"

#include "UI/TextWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"


MOE_BEGIN_NAMESPACE

struct VkTextWidget : public TextWidget, public VkWidget {
public:
    VkTextWidget(U32StringView text = U"", FontId fontId = NULL_FONT_ID, float fontSize = 16.f, const Color& style = Colors::White)
        : TextWidget(text, fontSize, style), m_fontId(fontId), m_font(getFontPtr(fontId)) {}

    ~VkTextWidget() override = default;

    LayoutSize preferredSize(const LayoutConstraints& constraints) const override;
    void layout(const LayoutRect& rectAssigned) override;

    void render(VulkanRenderObjectBus& renderer) override;

    void setFontId(FontId fontId) {
        m_fontId = fontId;
        m_font = getFontPtr(fontId);
    }

protected:
    FontId m_fontId{NULL_FONT_ID};

    VulkanFont* m_font{nullptr};

    static VulkanFont* getFontPtr(FontId fontId) {
        auto& engine = VulkanEngine::get();
        VulkanFontCache& fontCache = engine.m_caches.fontCache;
        return fontCache.get(fontId)->get();
    }
};

MOE_END_NAMESPACE