#pragma once

#include "UI/SpriteWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"


MOE_BEGIN_NAMESPACE

struct VkProgressBarWidget : public moe::SpriteWidget, public moe::VkWidget {
public:
    VkProgressBarWidget() = default;
    VkProgressBarWidget(LayoutSize preferredSize, Color backgroundColor, Color fillColor)
        : SpriteWidget(preferredSize), backgroundColor(backgroundColor), fillColor(fillColor) {}

    void setProgress(float progress) {
        m_progress = Math::clamp(progress, 0.0f, 1.0f);
    }

    float getProgress() const {
        return m_progress;
    }

    void render(VulkanRenderObjectBus& renderer) override;

private:
    Color backgroundColor{Color(0.2f, 0.2f, 0.2f, 1.0f)};
    Color fillColor{Colors::Green};
    float m_progress{0.0f};
};

MOE_END_NAMESPACE