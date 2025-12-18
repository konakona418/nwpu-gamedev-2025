#pragma once

#include "Render/Vulkan/VulkanEngineDrivers.hpp"

MOE_BEGIN_NAMESPACE

struct VkWidget {
public:
    virtual ~VkWidget() = default;

    virtual void render(VulkanRenderObjectBus& renderer) = 0;
};

MOE_END_NAMESPACE