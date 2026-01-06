#pragma once

#include "UI/SpriteWidget.hpp"
#include "UI/Vulkan/VkWidgetExt.hpp"

MOE_BEGIN_NAMESPACE

struct VkImageWidget : public SpriteWidget, public VkWidget {
public:
    VkImageWidget() = default;

    explicit VkImageWidget(ImageId imageId, LayoutSize spriteSize, Color tintColor = Colors::White, LayoutSize textureSize = {0, 0})
        : SpriteWidget(spriteSize), m_imageId(imageId), m_tintColor(tintColor) {
        if (textureSize.width > 0 && textureSize.height > 0) {
            m_textureSize = textureSize;
        } else {
            // use sprite size as texture size by default
            m_textureSize = spriteSize;
        }
    }

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
    LayoutSize m_textureSize{0, 0};
};

MOE_END_NAMESPACE