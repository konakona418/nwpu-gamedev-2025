#pragma once

#include "UI/BoxWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"

MOE_BEGIN_NAMESPACE

struct VkBoxWidget : public moe::BoxWidget, public moe::VkWidget {
public:
    explicit VkBoxWidget(
            moe::BoxLayoutDirection direction,
            moe::BoxJustify justify = moe::BoxJustify::Start,
            moe::BoxAlign align = moe::BoxAlign::Start)
        : moe::BoxWidget(direction, justify, align) {}

    void setBackgroundColor(const moe::Color& color) {
        m_backgroundColor = color;
    }

    const moe::Color& getBackgroundColor() const {
        return m_backgroundColor;
    }

    void render(VulkanRenderObjectBus& renderer) override;

private:
    moe::Color m_backgroundColor{moe::Colors::Transparent};
};

MOE_END_NAMESPACE