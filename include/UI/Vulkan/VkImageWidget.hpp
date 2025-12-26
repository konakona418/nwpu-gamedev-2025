#pragma once

#include "UI/SpriteWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"

MOE_BEGIN_NAMESPACE

struct VkImageWidget : public SpriteWidget, public VkWidget {
public:
    VkImageWidget() = default;

    explicit VkImageWidget(ImageId imageId, Color tintColor = Colors::White)
        : m_imageId(imageId), m_tintColor(tintColor) {}

    void render(VulkanRenderObjectBus& renderer) override;

    void setImageId(ImageId imageId) {
        m_imageId = imageId;
    }

    ImageId getImageId() const {
        return m_imageId;
    }

    void setTintColor(Color tintColor) {
        m_tintColor = tintColor;
    }

    Color getTintColor() const {
        return m_tintColor;
    }

private:
    ImageId m_imageId{NULL_IMAGE_ID};
    Color m_tintColor{Colors::White};
};

MOE_END_NAMESPACE