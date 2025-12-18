#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanInitializers.hpp"
#include "Render/Vulkan/VulkanLight.hpp"
#include "Render/Vulkan/VulkanLoaders.hpp"
#include "Render/Vulkan/VulkanMesh.hpp"
#include "Render/Vulkan/VulkanUtils.hpp"


#include <VkBootstrap.h>

#include "Render/Vulkan/VmaImpl.hpp"
#include "Render/Vulkan/VolkImpl.hpp"

#include <chrono>
#include <thread>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"


namespace moe {
    constexpr bool enableValidationLayers{true};

    VulkanEngine* g_engineInstance{nullptr};

    void DeletionQueue::pushFunction(std::function<void()>&& function) {
        deletors.push_back(std::move(function));
    }

    void DeletionQueue::flush() {
        for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
            (*it)();
        }
        deletors.clear();
    }

    VulkanEngine& VulkanEngine::get() {
        return *g_engineInstance;
    }

    void VulkanEngine::init() {
        MOE_ASSERT(g_engineInstance == nullptr, "engine instance already initialized");

        g_engineInstance = this;

        initWindow();
        initVulkanInstance();
        initSwapchain();
        initCommands();
        initSyncPrimitives();
        initDescriptors();

        initBindlessSet();
        initCaches();

        initPipelines();

        initImGUI();
        initIm3d();

        m_isInitialized = true;
    }

    void VulkanEngine::run() {
        bool shouldQuit = false;

        std::pair<uint32_t, uint32_t> newMetric;

        while (!shouldQuit) {
            glfwPollEvents();

            while (auto e = m_inputBus.pollEvent()) {
                if (e->is<WindowEvent::Close>()) {
                    Logger::debug("window closing...");
                    shouldQuit = true;
                }

                if (e->is<WindowEvent::Minimize>()) {
                    Logger::debug("window minimized");
                    m_stopRendering = true;
                } else if (e->is<WindowEvent::RestoreFromMinimize>()) {
                    Logger::debug("window restored from minimize");
                    m_stopRendering = false;
                }

                if (auto resize = e->getIf<WindowEvent::Resize>()) {
                    if (resize->width && resize->height) {
                        // Logger::debug("window resize: {}x{}", resize->width, resize->height);
                        m_resizeRequested = true;
                        newMetric = {resize->width, resize->height};
                    }
                }
            }

            if (m_resizeRequested) {
                Logger::debug("resize args: {}x{}", newMetric.first, newMetric.second);
                recreateSwapchain(newMetric.first, newMetric.second);

                m_resizeRequested = false;
            }

            if (m_stopRendering) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            ImGui::Render();

            draw();
        }
    }

    void VulkanEngine::beginFrame() {
        glfwPollEvents();

        std::pair<uint32_t, uint32_t> newMetric;

        for (auto& e: m_inputBus.m_pollingEvents) {
            if (e.is<WindowEvent::Minimize>()) {
                Logger::debug("window minimized");
                m_stopRendering = true;
            } else if (e.is<WindowEvent::RestoreFromMinimize>()) {
                Logger::debug("window restored from minimize");
                m_stopRendering = false;
            }

            if (auto resize = e.getIf<WindowEvent::Resize>()) {
                if (resize->width && resize->height) {
                    m_resizeRequested = true;
                    newMetric = {resize->width, resize->height};
                }
            }
        }

        if (m_resizeRequested) {
            Logger::debug("resize args: {}x{}", newMetric.first, newMetric.second);
            recreateSwapchain(newMetric.first, newMetric.second);

            m_resizeRequested = false;
        }

        // reset all shared dynamic states
        resetDynamicState();

        if (m_stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return;
        }
    }

    void VulkanEngine::endFrame() {
        if (m_stopRendering) {
            return;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        while (!m_imguiDrawQueue.empty()) {
            auto& fn = m_imguiDrawQueue.front();
            fn();
            m_imguiDrawQueue.pop();
        }

        ImGui::Render();

        draw();
    }

    void VulkanEngine::immediateSubmit(Function<void(VkCommandBuffer)>&& fn, Function<void()>&& postFn) {
        MOE_VK_CHECK(vkResetFences(m_device, 1, &m_immediateModeFence));
        MOE_VK_CHECK(vkResetCommandBuffer(m_immediateModeCommandBuffer, 0));

        auto commandBuffer = m_immediateModeCommandBuffer;
        VkCommandBufferBeginInfo beginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        MOE_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        fn(commandBuffer);

        MOE_VK_CHECK(vkEndCommandBuffer(commandBuffer));

        auto commandSubmitInfo = VkInit::commandBufferSubmitInfo(commandBuffer);
        auto submitInfo = VkInit::submitInfo(&commandSubmitInfo, nullptr, nullptr);

        MOE_VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, m_immediateModeFence));

        MOE_VK_CHECK(vkWaitForFences(m_device, 1, &m_immediateModeFence, VK_TRUE, UINT64_MAX));

        if (postFn) {
            postFn();
        }
    }

    VulkanAllocatedBuffer VulkanEngine::allocateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;

        bufferInfo.size = size;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo vmaAllocInfo{};
        vmaAllocInfo.usage = memoryUsage;
        vmaAllocInfo.flags =
                VMA_ALLOCATION_CREATE_MAPPED_BIT |
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        // todo: allow random access

        VulkanAllocatedBuffer buffer;
        MOE_VK_CHECK(
                vmaCreateBuffer(
                        m_allocator,
                        &bufferInfo,
                        &vmaAllocInfo,
                        &buffer.buffer,
                        &buffer.vmaAllocation,
                        &buffer.vmaAllocationInfo));

        if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            VkBufferDeviceAddressInfo deviceAddrInfo{};
            deviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            deviceAddrInfo.buffer = buffer.buffer;

            buffer.address = vkGetBufferDeviceAddress(m_device, &deviceAddrInfo);
        } else {
            buffer.address = 0;
        }

        return buffer;
    }

    VulkanAllocatedImage VulkanEngine::allocateImage(VkImageCreateInfo imageInfo) {
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.flags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VulkanAllocatedImage image{};
        image.imageExtent = imageInfo.extent;
        image.imageFormat = imageInfo.format;

        VkImageAspectFlags aspect =
                imageInfo.format == VK_FORMAT_D32_SFLOAT
                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                        : VK_IMAGE_ASPECT_COLOR_BIT;

        MOE_VK_CHECK(
                vmaCreateImage(g_engineInstance->m_allocator, &imageInfo, &allocInfo, &image.image, &image.vmaAllocation, nullptr));

        VkImageViewCreateInfo imageViewInfo = VkInit::imageViewCreateInfo(image.imageFormat, image.image, aspect);
        imageViewInfo.subresourceRange.levelCount = imageInfo.mipLevels;

        // note: this is a bit hacky (designed solely for CSM shadow maps), but it works anyway
        if (imageInfo.arrayLayers > 1) {
            Logger::debug("creating image view for image with {} array layers, automatically using TYPE_2D_ARRAY", imageInfo.arrayLayers);
            imageViewInfo.subresourceRange.baseArrayLayer = 0;
            imageViewInfo.subresourceRange.layerCount = imageInfo.arrayLayers;
            imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

        MOE_VK_CHECK(vkCreateImageView(m_device, &imageViewInfo, nullptr, &image.imageView));

        return image;
    }

    VulkanAllocatedImage VulkanEngine::allocateImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap) {
        VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(format, usage, extent);

        if (mipmap) {
            imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
        } else {
            imageInfo.mipLevels = 1;
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.flags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VulkanAllocatedImage image{};
        image.imageExtent = extent;
        image.imageFormat = format;

        MOE_VK_CHECK(
                vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &image.image, &image.vmaAllocation, nullptr));

        VkImageAspectFlags aspect =
                format == VK_FORMAT_D32_SFLOAT
                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                        : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageViewCreateInfo imageViewInfo = VkInit::imageViewCreateInfo(format, image.image, aspect);
        imageViewInfo.subresourceRange.levelCount = imageInfo.mipLevels;

        MOE_VK_CHECK(vkCreateImageView(m_device, &imageViewInfo, nullptr, &image.imageView));

        return image;
    }

    VulkanAllocatedImage VulkanEngine::allocateImage(void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap) {
        size_t imageSize = extent.width * extent.height * extent.depth * VkUtils::getChannelsFromFormat(format);
        VulkanAllocatedBuffer stagingBuffer = allocateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        std::memcpy(stagingBuffer.vmaAllocationInfo.pMappedData, data, imageSize);

        VulkanAllocatedImage image = allocateImage(
                extent, format,
                usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                mipmap);
        immediateSubmit([&](VkCommandBuffer cmd) {
            VkUtils::transitionImage(
                    cmd, image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = extent;

            vkCmdCopyBufferToImage(
                    cmd,
                    stagingBuffer.buffer,
                    image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);
            if (mipmap) {
                VkUtils::generateMipmaps(cmd, image.image, VkExtent2D{extent.width, extent.height});
            } else {
                VkUtils::transitionImage(
                        cmd, image.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });

        destroyBuffer(stagingBuffer);

        return image;
    }

    VulkanAllocatedImage VulkanEngine::allocateCubeMapImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap) {
        VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(format, usage, extent);
        imageInfo.arrayLayers = 6;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;

        if (mipmap) {
            imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
        } else {
            imageInfo.mipLevels = 1;
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.flags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VulkanAllocatedImage image{};
        image.imageExtent = extent;
        image.imageFormat = format;

        MOE_VK_CHECK(
                vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &image.image, &image.vmaAllocation, nullptr));

        VkImageAspectFlags aspect =
                format == VK_FORMAT_D32_SFLOAT
                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                        : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageViewCreateInfo imageViewInfo = VkInit::imageViewCreateInfo(format, image.image, aspect);
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        imageViewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
        imageViewInfo.subresourceRange.layerCount = 6;

        MOE_VK_CHECK(vkCreateImageView(m_device, &imageViewInfo, nullptr, &image.imageView));

        return image;
    }

    VulkanAllocatedImage VulkanEngine::allocateCubeMapImage(Array<void*, 6> data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap) {
        size_t imageSize = extent.width * extent.height * extent.depth * VkUtils::getChannelsFromFormat(format);
        VulkanAllocatedBuffer stagingBuffer = allocateBuffer(imageSize * 6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        MOE_ASSERT(stagingBuffer.vmaAllocationInfo.pMappedData != nullptr, "Failed to map staging buffer");
        for (size_t i = 0; i < 6; ++i) {
            std::memcpy(static_cast<uint8_t*>(stagingBuffer.vmaAllocationInfo.pMappedData) + imageSize * i, data[i], imageSize);
        }

        VulkanAllocatedImage image = allocateCubeMapImage(
                extent, format,
                usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                mipmap);
        immediateSubmit([&](VkCommandBuffer cmd) {
            VkUtils::transitionImage(
                    cmd, image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            Array<VkBufferImageCopy, 6> copyRegions{};
            for (uint32_t i = 0; i < 6; ++i) {
                copyRegions[i].bufferOffset = imageSize * i;
                copyRegions[i].bufferRowLength = 0;
                copyRegions[i].bufferImageHeight = 0;

                copyRegions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegions[i].imageSubresource.mipLevel = 0;
                copyRegions[i].imageSubresource.baseArrayLayer = i;
                copyRegions[i].imageSubresource.layerCount = 1;
                copyRegions[i].imageExtent = extent;
            }

            vkCmdCopyBufferToImage(
                    cmd,
                    stagingBuffer.buffer,
                    image.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    static_cast<uint32_t>(copyRegions.size()),
                    copyRegions.data());
            if (mipmap) {
                VkUtils::generateMipmaps(cmd, image.image, VkExtent2D{extent.width, extent.height});
            } else {
                VkUtils::transitionImage(
                        cmd, image.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });

        destroyBuffer(stagingBuffer);

        return image;
    }

    Optional<VulkanAllocatedImage> VulkanEngine::loadImageFromFile(StringView filename, VkFormat format, VkImageUsageFlags usage, bool mipmap) {
        size_t expectedChannels = VkUtils::getChannelsFromFormat(format);
        MOE_ASSERT(expectedChannels <= 4, "Image format must have 4 channels or less");

        int width, height, channels;
        auto rawImage = VkLoaders::loadImage(filename, &width, &height, &channels, expectedChannels);

        if (channels != expectedChannels) {
            MOE_ASSERT(channels <= expectedChannels, "Image has more channels than the required format supports");
            Logger::warn("Image channels({}) does not match required format channels({})", channels, expectedChannels);
        }

        if (!rawImage) {
            Logger::error("Failed to open image: {}", filename);
            return {std::nullopt};
        }

        VulkanAllocatedImage image = allocateImage(
                rawImage.get(),
                {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                format,
                usage,
                mipmap);

        return image;
    }

    Optional<VulkanAllocatedImage> VulkanEngine::loadImageFromMemory(Span<uint8_t> imageData, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap) {
        // ! this is for internal use only
        // ! loading arbitary image from memory is not safe without knowing the format and size beforehand
        if (imageData.empty()) {
            Logger::error("Image data is empty");
            return {std::nullopt};
        }

        VulkanAllocatedImage image = allocateImage(
                imageData.data(),
                {extent.width, extent.height, 1},
                format,
                usage,
                mipmap);

        return image;
    }

    void VulkanEngine::destroyImage(VulkanAllocatedImage& image) {
        vkDestroyImageView(m_device, image.imageView, nullptr);
        vmaDestroyImage(m_allocator, image.image, image.vmaAllocation);
    }

    void VulkanEngine::destroyBuffer(VulkanAllocatedBuffer& buffer) {
        vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.vmaAllocation);
    }

    VulkanGPUMeshBuffer VulkanEngine::uploadMesh(Span<uint32_t> indices, Span<Vertex> vertices, Span<SkinningData> skinningData) {
        const size_t vertBufferSize = vertices.size() * sizeof(Vertex);
        const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
        const size_t skinningDataBufferSize = skinningData.size() * sizeof(SkinningData);

        VulkanGPUMeshBuffer surface;
        surface.indexCount = static_cast<uint32_t>(indices.size());
        surface.vertexCount = static_cast<uint32_t>(vertices.size());

        surface.vertexBuffer = allocateBuffer(
                vertBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY);

        {
            VkBufferDeviceAddressInfo deviceAddrInfo{};
            deviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            deviceAddrInfo.buffer = surface.vertexBuffer.buffer;

            surface.vertexBufferAddr = vkGetBufferDeviceAddress(m_device, &deviceAddrInfo);
        }

        surface.indexBuffer = allocateBuffer(
                indexBufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY);

        bool hasSkinningData = !skinningData.empty();
        surface.hasSkinningData = hasSkinningData;

        size_t stagingBufferSize = 0;

        if (hasSkinningData) {
            Logger::info("Mesh has skinning data, size {} bytes", skinningDataBufferSize);

            // no need to initialize skinned vertex buffer here, will be done in skinning pipeline
            stagingBufferSize = indexBufferSize + vertBufferSize + skinningDataBufferSize;

            surface.skinningDataBuffer = allocateBuffer(
                    skinningDataBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);
            {
                VkBufferDeviceAddressInfo deviceAddrInfo{};
                deviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                deviceAddrInfo.buffer = surface.skinningDataBuffer.buffer;
                surface.skinningDataBufferAddr = vkGetBufferDeviceAddress(m_device, &deviceAddrInfo);
            }

            surface.skinnedVertexBuffer = allocateBuffer(
                    vertBufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);
            {
                VkBufferDeviceAddressInfo deviceAddrInfo{};
                deviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                deviceAddrInfo.buffer = surface.skinnedVertexBuffer.buffer;
                surface.skinnedVertexBufferAddr = vkGetBufferDeviceAddress(m_device, &deviceAddrInfo);
            }
        } else {
            stagingBufferSize = indexBufferSize + vertBufferSize;

            surface.skinningDataBuffer = {};
            surface.skinningDataBufferAddr = 0;

            surface.skinnedVertexBuffer = {};
            surface.skinnedVertexBufferAddr = 0;
        }

        VulkanAllocatedBuffer stagingBuffer = allocateBuffer(
                stagingBufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY);

        void* data = stagingBuffer.vmaAllocationInfo.pMappedData;

        std::memcpy(data, vertices.data(), vertBufferSize);
        std::memcpy(static_cast<uint8_t*>(data) + vertBufferSize,
                    indices.data(), indexBufferSize);

        if (hasSkinningData) {
            std::memcpy(static_cast<uint8_t*>(data) + vertBufferSize + indexBufferSize,
                        skinningData.data(), skinningDataBufferSize);
        }

        immediateSubmit([&, hasSkinningData](VkCommandBuffer cmdBuffer) {
            VkBufferCopy vertCopy{};
            vertCopy.srcOffset = 0;
            vertCopy.dstOffset = 0;
            vertCopy.size = vertBufferSize;

            vkCmdCopyBuffer(cmdBuffer,
                            stagingBuffer.buffer, surface.vertexBuffer.buffer,
                            1, &vertCopy);

            VkBufferCopy indexCopy{};
            indexCopy.srcOffset = vertBufferSize;
            indexCopy.dstOffset = 0;
            indexCopy.size = indexBufferSize;

            vkCmdCopyBuffer(cmdBuffer,
                            stagingBuffer.buffer, surface.indexBuffer.buffer,
                            1, &indexCopy);

            if (hasSkinningData) {
                VkBufferCopy skinningCopy{};
                skinningCopy.srcOffset = vertBufferSize + indexBufferSize;
                skinningCopy.dstOffset = 0;
                skinningCopy.size = skinningDataBufferSize;

                vkCmdCopyBuffer(cmdBuffer,
                                stagingBuffer.buffer, surface.skinningDataBuffer.buffer,
                                1, &skinningCopy);
            }
        });

        destroyBuffer(stagingBuffer);

        return surface;
    }

    void VulkanEngine::draw() {
        auto& currentFrame = getCurrentFrame();
        auto currentFrameIndex = getCurrentFrameIndex();

        MOE_VK_CHECK_MSG(
                vkWaitForFences(
                        m_device,
                        1, &currentFrame.inFlightFence,
                        VK_TRUE,
                        VkUtils::secsToNanoSecs(1.0f)),
                "Failed to wait for fence");

        currentFrame.deletionQueue.flush();

        currentFrame.descriptorAllocator.clearPools(m_device);

        MOE_VK_CHECK_MSG(
                vkResetFences(m_device, 1, &currentFrame.inFlightFence),
                "Failed to reset fence");

        uint32_t swapchainImageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(
                m_device,
                m_swapchain,
                VkUtils::secsToNanoSecs(1.0f),
                currentFrame.imageAvailableSemaphore,
                VK_NULL_HANDLE,
                &swapchainImageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            // ! note that with resize event hooked, no need to set resize flag here.
            // ! another note: some drivers can fix suboptimal cases automatically, which is quite absurd.
            Logger::warn("vkAcquireNextImageKHR returned VK_ERROR_OUT_OF_DATE_KHR, forcing resize.");
            m_resizeRequested = true;
            return;
        } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            MOE_LOG_AND_THROW("Failed to acquire next swapchain image");
        }

        VkCommandBuffer commandBuffer = currentFrame.mainCommandBuffer;

        MOE_VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

        VkCommandBufferBeginInfo beginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        MOE_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        // ! begin skinning. as we need to upload joint matrices from cpu to gpu, we do it first.
        m_pipelines.skinningPipeline.beginFrame(currentFrameIndex);

        auto& computeSkinCommands = m_renderBus.getComputeSkinCommands();
        ComputeSkinHandleId handleIdCounter = 0;
        ComputeSkinHandleId maxHandleId = m_renderBus.getNumComputeSkinCommands();

        for (auto& command: computeSkinCommands) {
            MOE_ASSERT(handleIdCounter < maxHandleId, "Compute skin command handle id out of range");
            auto handleId = handleIdCounter++;

            // initialize with invalid index
            m_renderBus.setComputeSkinMatrix(handleId, INVALID_JOINT_MATRIX_START_INDEX);

            auto renderable = m_caches.objectCache.get(command.renderableId);
            if (!renderable.has_value()) {
                Logger::warn("Renderable with id {} not found in cache", command.renderableId);
                continue;
            }

            auto animation = m_caches.animationCache.get(command.animationId);
            if (!animation.has_value()) {
                Logger::warn("Animation with id {} not found in cache", command.animationId);
                continue;
            }

            if (!renderable->get()->hasFeature<VulkanRenderableFeature::HasSkeletalAnimation>()) {
                Logger::warn("Renderable with id {} does not have skeletal animation feature", command.renderableId);
                continue;
            }

            auto* skeletal = renderable->get()->as<VulkanSkeletalAnimation>();
            // ! fixme: only support one skeleton for now
            auto& skeleton = skeletal->getSkeletons()[0];

            Vector<glm::mat4> jointMatrices;
            calculateJointMatrices(jointMatrices, skeleton, *animation.value(), command.time);
            auto offset = m_pipelines.skinningPipeline.appendJointMatrices(jointMatrices, currentFrameIndex);

            m_renderBus.setComputeSkinMatrix(handleId, offset);
        }

        // ! load scene render packets

        auto& renderTarget =
                *m_caches.renderTargetCache.get(m_defaultRenderTargetId).value();
        auto& renderView = *m_caches.renderViewCache.get(m_defaultRenderViewId).value();

        auto drawImageId = renderView.drawImageId;
        auto depthImageId = renderView.depthImageId;
        auto resolveImageId = renderView.msaaResolveImageId;

        auto drawImage = *m_caches.imageCache.getImage(drawImageId);
        auto depthImage = *m_caches.imageCache.getImage(depthImageId);
        VulkanAllocatedImage resolveImage;
        if (resolveImageId != NULL_IMAGE_ID) {
            resolveImage = *m_caches.imageCache.getImage(resolveImageId);
        }

        // clear previous frame's render packets
        renderTarget.resetDynamicState();
        auto& packets = renderTarget.renderPackets;

        static constexpr size_t MAX_EXPECTED_RENDER_PACKETS = 2048;
        if (packets.size() > MAX_EXPECTED_RENDER_PACKETS) {
            Logger::warn("Render packets size {} exceeds max expected {}, is this normal or is the packets vector not cleared properly?",
                         packets.size(), MAX_EXPECTED_RENDER_PACKETS);
        }

        for (auto& renderCommands: m_renderBus.getRenderCommands()) {
            auto id = renderCommands.renderableId;
            if (auto renderable = m_caches.objectCache.get(id)) {
                // todo: upload skeleton matrices if present, and set the values in render packets
                size_t offset = INVALID_JOINT_MATRIX_START_INDEX;
                if (renderCommands.computeHandle != NULL_COMPUTE_SKIN_HANDLE_ID) {
                    offset = m_renderBus.getComputeSkinMatrix(renderCommands.computeHandle);
                }
                VulkanDrawContext ctx = NULL_DRAW_CONTEXT;
                ctx.jointMatrixStartIndex = offset;
                renderable->get()->updateTransform(renderCommands.transform.getMatrix());
                renderable->get()->gatherRenderPackets(packets, ctx);
            } else {
                Logger::warn("Renderable with id {} not found in cache", id);
                continue;
            }
        }

        // ! compute

        {
            // barrier previous read - compute
            const auto barrier = VkMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            };
            const auto dependencyInfo = VkDependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .memoryBarrierCount = 1,
                    .pMemoryBarriers = &barrier,
            };
            vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        }

        // ! todo: load skinning matrices from cpu to gpu
        m_pipelines.skinningPipeline.compute(commandBuffer, m_caches.meshCache, packets, currentFrameIndex);

        {
            // barrier compute - csm shadow
            const auto barrier = VkMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            };
            const auto dependencyInfo = VkDependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .memoryBarrierCount = 1,
                    .pMemoryBarriers = &barrier,
            };
            vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        }

        auto& defaultCamera = getDefaultCamera();

        // ! illumination information upload
        m_illuminationBus.uploadToGPU(commandBuffer, currentFrameIndex);

        // ! shadow
        m_pipelines.csmPipeline.setShadowMapCameraScale(m_shadowMapCameraScale);
        m_pipelines.csmPipeline.draw(
                commandBuffer,
                m_caches.meshCache,
                packets,
                defaultCamera,
                m_illuminationBus.getSunlight().direction);

        // ! initialize scene data

        auto cameraView = defaultCamera.viewMatrix();
        auto cameraProjection = defaultCamera.projectionMatrix((float) m_drawExtent.width / (float) m_drawExtent.height);
        //cameraProjection[1][1] *= -1;
        auto cameraViewProjection = cameraProjection * cameraView;
        auto cameraPosition = glm::vec4(defaultCamera.getPosition(), 1.0f);
        VulkanGPUSceneData sceneData{
                .view = cameraView,
                .projection = cameraProjection,
                .viewProjection = cameraViewProjection,
                .invView = glm::inverse(cameraView),
                .invProjection = glm::inverse(cameraProjection),
                .invViewProjection = glm::inverse(cameraViewProjection),
                .cameraPosition = cameraPosition,
                .ambientColor = m_illuminationBus.getAmbientColorStrength(),
                .materialBuffer = m_caches.materialCache.getMaterialBufferAddress(),
                .lightBuffer = m_illuminationBus.getGPUBuffer().getBuffer().address,
                .numLights = (uint32_t) m_illuminationBus.getNumLights(),
                .skyboxId = m_pipelines.skyBoxImageId,
        };

        {
            sceneData.csmShadowMapId = m_pipelines.csmPipeline.getShadowMapImageId();
            for (int i = 0; i < 4; ++i) {
                sceneData.csmShadowMapLightTransform[i] = m_pipelines.csmPipeline.m_cascadeLightTransforms[i];
            }
            sceneData.shadowMapCascadeSplits = glm::vec4{
                    m_pipelines.csmPipeline.m_cascadeFarPlaneZs[0],
                    m_pipelines.csmPipeline.m_cascadeFarPlaneZs[1],
                    m_pipelines.csmPipeline.m_cascadeFarPlaneZs[2],
                    m_pipelines.csmPipeline.m_cascadeFarPlaneZs[3],
            };
        }

        m_pipelines.sceneDataBuffer.upload(commandBuffer, &sceneData, currentFrameIndex, sizeof(VulkanGPUSceneData));

        // ! begin of main render pass

        //! fixme: general image layout is not an optimal choice.
        VkUtils::transitionImage(
                commandBuffer, drawImage.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        VkUtils::transitionImage(
                commandBuffer, depthImage.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        if (isMultisamplingEnabled()) {
            VkUtils::transitionImage(
                    commandBuffer, resolveImage.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }

        // todo: sync with last read
        m_pipelines.gBufferPipeline.draw(
                commandBuffer,
                m_caches.meshCache, m_caches.materialCache,
                packets, m_pipelines.sceneDataBuffer.getBuffer());

        auto clearColor = renderView.clearColor;
        VkClearValue clearValue = {
                .color = {
                        clearColor.r,
                        clearColor.g,
                        clearColor.b,
                        clearColor.a,
                },
        };
        //VkClearValue depthClearValue = {.depthStencil = {1.0f, 0}};
        auto colorAttachment = VkInit::renderingAttachmentInfo(drawImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        if (isMultisamplingEnabled()) {
            colorAttachment.resolveImageView = resolveImage.imageView;
            colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;// ! msaa x4
            // resolve msaa x4 image -> 1 sample resolved image
        }

        //auto depthAttachment = VkInit::renderingAttachmentInfo(m_depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        auto renderInfo = VkInit::renderingInfo(m_drawExtent, &colorAttachment, nullptr);
        vkCmdBeginRendering(commandBuffer, &renderInfo);

        /*m_pipelines.meshPipeline.draw(
                commandBuffer,
                m_caches.meshCache,
                m_caches.materialCache,
                packets,
                m_pipelines.sceneDataBuffer);

        m_pipelines.skyBoxPipeline.draw(commandBuffer, getDefaultCamera());*/

        m_pipelines.deferredLightingPipeline.draw(
                commandBuffer,
                m_pipelines.sceneDataBuffer.getBuffer(),
                m_pipelines.gBufferPipeline.gDepthId,
                m_pipelines.gBufferPipeline.gAlbedoId,
                m_pipelines.gBufferPipeline.gNormalId,
                m_pipelines.gBufferPipeline.gORMAId,
                m_pipelines.gBufferPipeline.gEmissiveId);


        vkCmdEndRendering(commandBuffer);

        // ! im3d
        // im3d pipeline requires the draw image to be in color attachment optimal layout
        VkUtils::transitionImage(
                commandBuffer,
                drawImage.image,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // barrier previous write
        VkUtils::transitionImage(
                commandBuffer,
                m_pipelines.gBufferPipeline.gDepth.image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        // acquire input state
        VulkanIm3dDriver::MouseState mouseState{};
        {
            mouseState.leftButtonDown =
                    glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            double x, y;
            glfwGetCursorPos(m_window, &x, &y);
            mouseState.position = glm::vec2{static_cast<float>(x), static_cast<float>(y)};
        }

        // this requires the region of the real (i.e. scaled for high-dpi, e.g. render image -> swapchain) framebuffer
        auto extent = glm::vec2{m_swapchainExtent.width, m_swapchainExtent.height};
        m_im3dDriver.beginFrame(
                extent,
                &defaultCamera,
                mouseState);

        while (!m_im3dDrawQueue.empty()) {
            auto& cmd = m_im3dDrawQueue.front();
            cmd();
            m_im3dDrawQueue.pop();
        }

        m_im3dDriver.endFrame();

        m_im3dDriver.render(
                commandBuffer,
                &defaultCamera,
                drawImage.imageView,
                m_pipelines.gBufferPipeline.gDepth.imageView,// requires depth info
                m_drawExtent);

        // ! sprites

        // ! fixme: the sprite layer should not use fxaa

        // barrier prev write
        VkUtils::transitionImage(
                commandBuffer,
                drawImage.image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL);

        auto& sprites = m_renderBus.getSpriteRenderCommands();
        m_pipelines.spritePipeline.draw(
                commandBuffer,
                m_caches.meshCache,
                sprites,
                m_defaultSpriteCamera->getViewProjectionMatrix(),
                drawImage);

        // ! post fx

        m_pipelines.postFxGraph.copyToInput(commandBuffer, "#input", drawImage, VK_IMAGE_LAYOUT_GENERAL);
        m_pipelines.postFxGraph.exec(commandBuffer);
        m_pipelines.postFxGraph.copyFromOutput(commandBuffer, drawImage, VK_IMAGE_LAYOUT_GENERAL);

        VkUtils::transitionImage(
                commandBuffer, drawImage.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkUtils::transitionImage(
                commandBuffer, m_swapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkUtils::copyImage(
                commandBuffer, drawImage.image, m_swapchainImages[swapchainImageIndex],
                VkExtent2D{drawImage.imageExtent.width, drawImage.imageExtent.height},
                VkExtent2D{m_swapchainExtent.width, m_swapchainExtent.height});

        VkUtils::transitionImage(
                commandBuffer, m_swapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        drawImGUI(commandBuffer, m_swapchainImageViews[swapchainImageIndex]);

        VkUtils::transitionImage(
                commandBuffer, m_swapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        MOE_VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkCommandBufferSubmitInfo submitInfo = VkInit::commandBufferSubmitInfo(commandBuffer);
        VkSemaphoreSubmitInfo waitInfo =
                VkInit::semaphoreSubmitInfo(
                        currentFrame.imageAvailableSemaphore,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
        VkSemaphoreSubmitInfo signalInfo =
                VkInit::semaphoreSubmitInfo(
                        m_perSwapchainImageData[swapchainImageIndex].renderFinishedSemaphore,
                        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

        VkSubmitInfo2 submitInfo2 = VkInit::submitInfo(&submitInfo, &waitInfo, &signalInfo);
        MOE_VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo2, currentFrame.inFlightFence));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;

        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &m_perSwapchainImageData[swapchainImageIndex].renderFinishedSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &swapchainImageIndex;

        VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
            // ! note that with resize event hooked, no need to set resize flag here.
            Logger::warn("vkQueuePresentKHR returned VK_ERROR_OUT_OF_DATE_KHR, forcing resize.");
            m_resizeRequested = true;
        }

        m_frameNumber++;
    }

    void VulkanEngine::drawImGUI(VkCommandBuffer commandBuffer, VkImageView drawTarget) {
        auto attachment = VkInit::renderingAttachmentInfo(drawTarget, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto renderInfo = VkInit::renderingInfo(m_swapchainExtent, &attachment, nullptr);

        vkCmdBeginRendering(commandBuffer, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        vkCmdEndRendering(commandBuffer);
    }

    void VulkanEngine::cleanup() {
        if (m_isInitialized) {
            Logger::info("Cleaning up Vulkan engine...");

            vkDeviceWaitIdle(m_device);

            for (auto& frame: m_frames) {
                vkDestroyCommandPool(m_device, frame.commandPool, nullptr);

                vkDestroySemaphore(m_device, frame.imageAvailableSemaphore, nullptr);
                // vkDestroySemaphore(m_device, frame.renderFinishedSemaphore, nullptr);
                vkDestroyFence(m_device, frame.inFlightFence, nullptr);

                frame.deletionQueue.flush();
            }

            for (auto& data: m_perSwapchainImageData) {
                vkDestroySemaphore(m_device, data.renderFinishedSemaphore, nullptr);
            }

            m_mainDeletionQueue.flush();

            destroySwapchain();

            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

            vkDestroyDevice(m_device, nullptr);

            vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
            vkDestroyInstance(m_instance, nullptr);

            glfwDestroyWindow(m_window);
            m_window = nullptr;

            Logger::info("Vulkan engine cleaned up.");
        }

        g_engineInstance = nullptr;
    }

    void VulkanEngine::initWindow() {
        if (!glfwInit()) {
            MOE_LOG_AND_THROW("Fail to initialize glfw");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // disable resize for simplicity
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(m_windowExtent.width, m_windowExtent.height, "Moe Graphics Engine", nullptr, nullptr);

        if (!m_window) {
            MOE_LOG_AND_THROW("Fail to create GLFWwindow");
        }

        m_inputBus.m_isKeyPressedFunc = [this](int key) -> bool {
            return isKeyPressed(key);
        };

        m_inputBus.m_setMouseValidFunc = [this](bool valid) {
            if (!valid) {
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                if (glfwRawMouseMotionSupported()) {
                    glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
                }
            } else {
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                if (glfwRawMouseMotionSupported()) {
                    glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
                }
            }
        };

        glfwSetWindowUserPointer(m_window, this);

        glfwSetWindowCloseCallback(m_window, [](auto* window) {
            auto* engine = static_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
            engine->queueEvent({WindowEvent::Close{}});
        });

        glfwSetWindowIconifyCallback(m_window, [](auto* window, int iconified) {
            auto* engine = static_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
            if (iconified) {
                engine->queueEvent({WindowEvent::Minimize{}});
            } else {
                engine->queueEvent({WindowEvent::RestoreFromMinimize{}});
            }
        });

        glfwSetWindowSizeCallback(m_window, [](auto* window, int width, int height) {
            auto* engine = static_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));

            auto event =
                    WindowEvent::Resize{
                            static_cast<uint32_t>(width),
                            static_cast<uint32_t>(height),
                    };

            if (!engine->m_inputBus.m_pollingEvents.empty()) {
                if (engine->m_inputBus.m_pollingEvents.back().is<WindowEvent::Resize>()) {
                    // remove previous resize event to avoid flooding
                    engine->m_inputBus.m_pollingEvents.back() = {event};
                }
            }

            engine->queueEvent({event});
        });

        glfwSetKeyCallback(m_window, [](auto* window, int key, int scancode, int action, int mods) {
            auto* engine = static_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));

            if (action == GLFW_PRESS) {
                engine->queueEvent({WindowEvent::KeyDown{(uint32_t) key}});
            } else if (action == GLFW_REPEAT) {
                engine->queueEvent({WindowEvent::KeyRepeat{(uint32_t) key}});
            } else if (action == GLFW_RELEASE) {
                engine->queueEvent({WindowEvent::KeyUp{(uint32_t) key}});
            }
        });

        glfwSetCursorPosCallback(m_window, [](auto* window, double xpos, double ypos) {
            auto* engine = static_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));

            if (engine->m_firstMouse) {
                engine->m_lastMousePos = {static_cast<float>(xpos), static_cast<float>(ypos)};
                engine->m_firstMouse = false;
            }

            float dx = static_cast<float>(xpos - engine->m_lastMousePos.first);
            float dy = static_cast<float>(ypos - engine->m_lastMousePos.second);
            engine->queueEvent({WindowEvent::MouseMove{dx, dy}});

            engine->m_lastMousePos = {static_cast<float>(xpos), static_cast<float>(ypos)};
        });
    }

    void VulkanEngine::initVulkanInstance() {
        vkb::InstanceBuilder builder;

        Logger::info("Initializing Volk...");
        MOE_VK_CHECK(volkInitialize());

        Logger::info("Initializing Vulkan instance...");
        Logger::info("Using Vulkan api version {}.{}.{}", 1, 3, 0);

        auto result =
                builder.set_app_name("Moe Graphics Engine")
                        .request_validation_layers(enableValidationLayers)
                        .add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
                        .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
                        .set_debug_callback(
                                [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                   VkDebugUtilsMessageTypeFlagsEXT type,
                                   const VkDebugUtilsMessengerCallbackDataEXT* data,
                                   void* ctx) -> VkBool32 {
                                    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                                        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                                            Logger::error("Validation Layer: {}", data->pMessage);
                                        } else {
                                            Logger::warn("Validation Layer: {}", data->pMessage);
                                        }
                                    } else {
                                        Logger::info("Validation Layer: {}", data->pMessage);
                                    }
                                    return VK_FALSE;
                                })
                        .require_api_version(1, 3, 0)
                        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
                        .build();

        if (!result) {
            MOE_LOG_AND_THROW("Fail to create Vulkan instance");
        }

        auto vkbInstance = *result;

        m_instance = vkbInstance.instance;

        Logger::info("Loading Volk for Vulkan instance...");
        volkLoadInstance(m_instance);

        m_debugMessenger = vkbInstance.debug_messenger;

        Logger::info("Vulkan instance created.");

        MOE_VK_CHECK(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface));

        VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures = {
                .imageCubeArray = VK_TRUE,
                .geometryShader = VK_TRUE,
                .depthClamp = VK_TRUE,
                .samplerAnisotropy = VK_TRUE,
                .shaderStorageImageMultisample = VK_TRUE,
        };

        VkPhysicalDeviceVulkan13Features vkPhysicalDeviceVulkan13Features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .synchronization2 = VK_TRUE,
                .dynamicRendering = VK_TRUE,
        };

        VkPhysicalDeviceVulkan12Features vkPhysicalDeviceVulkan12Features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .descriptorIndexing = VK_TRUE,
                // used for bindless descriptors
                .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
                .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
                .descriptorBindingPartiallyBound = VK_TRUE,
                .descriptorBindingVariableDescriptorCount = VK_TRUE,
                .runtimeDescriptorArray = VK_TRUE,
                .scalarBlockLayout = VK_TRUE,
                .bufferDeviceAddress = VK_TRUE,
        };

        vkb::PhysicalDeviceSelector physicalDeviceSelector{vkbInstance};
        auto selectionResult =
                physicalDeviceSelector.set_minimum_version(1, 3)
                        .set_required_features(vkPhysicalDeviceFeatures)
                        .set_required_features_12(vkPhysicalDeviceVulkan12Features)
                        .set_required_features_13(vkPhysicalDeviceVulkan13Features)
                        .add_required_extension("VK_EXT_descriptor_indexing")
                        .add_required_extension("VK_KHR_shader_non_semantic_info")
                        .set_surface(m_surface)
                        .select();

        if (!selectionResult) {
            MOE_LOG_AND_THROW("Failed to select a valid physical device with proper features.");
        }

        auto vkbPhysicalDevice = *selectionResult;
        m_physicalDevice = vkbPhysicalDevice.physical_device;

        Logger::info("Using GPU: {}", vkbPhysicalDevice.properties.deviceName);

        vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
        auto deviceResult = deviceBuilder.build();

        if (!deviceResult) {
            MOE_LOG_AND_THROW("Failed to create a logical device.");
        }

        m_device = deviceResult->device;
        Logger::info("Loading Volk for Vulkan device...");
        volkLoadDevice(m_device);

        m_graphicsQueue = deviceResult->get_queue(vkb::QueueType::graphics).value();
        m_graphicsQueueFamilyIndex = deviceResult->get_queue_index(vkb::QueueType::graphics).value();

        Logger::info("Creating VMA instance...");
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = m_physicalDevice;
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_instance;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        // pass in necessary function ptrs
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator(&allocatorInfo, &m_allocator);

        m_mainDeletionQueue.pushFunction([&] {
            vmaDestroyAllocator(m_allocator);
        });
    }

    void VulkanEngine::initImGUI() {
        VkDescriptorPoolSize poolSizes[]{
                {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = (uint32_t) std::size(poolSizes);
        poolInfo.pPoolSizes = poolSizes;

        VkDescriptorPool imguiPool;
        MOE_VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &imguiPool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& imGuiIo = ImGui::GetIO();
        imGuiIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(m_window, true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = m_instance;
        initInfo.PhysicalDevice = m_physicalDevice;
        initInfo.Device = m_device;
        initInfo.Queue = m_graphicsQueue;
        initInfo.DescriptorPool = imguiPool;
        initInfo.MinImageCount = FRAMES_IN_FLIGHT;
        initInfo.ImageCount = FRAMES_IN_FLIGHT;
        initInfo.UseDynamicRendering = true;

        initInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapchainImageFormat;

        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&initInfo);

        m_mainDeletionQueue.pushFunction([=]() {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
        });
    }

    void VulkanEngine::initIm3d() {
        m_im3dDriver.init(
                *this,
                m_drawImageFormat,
                m_depthImageFormat);

        m_mainDeletionQueue.pushFunction([&]() {
            m_im3dDriver.destroy();
        });
    }

    void VulkanEngine::initSwapchain() {
        createSwapchain(m_windowExtent.width, m_windowExtent.height);

        VkExtent3D drawExtent = {
                m_windowExtent.width,
                m_windowExtent.height,
                1};

        m_drawExtent = {drawExtent.width, drawExtent.height};

        // ! the creation of post fx images are not placed here
        // ! see initPostFXImages()
    }

    void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
        vkb::SwapchainBuilder swapchainBuilder{m_physicalDevice, m_device, m_surface};

        m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        auto swapchainResult =
                swapchainBuilder.set_desired_format({m_swapchainImageFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                        .set_desired_extent(width, height)
                        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                        .build();

        if (!swapchainResult) {
            MOE_LOG_AND_THROW("Failed to create swapchain.");
        }

        auto vkbSwapchain = swapchainResult.value();
        m_swapchain = vkbSwapchain.swapchain;
        m_swapchainExtent = vkbSwapchain.extent;
        m_swapchainImages = vkbSwapchain.get_images().value();
        m_swapchainImageViews = vkbSwapchain.get_image_views().value();
        m_swapchainImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    }

    void VulkanEngine::destroySwapchain() {
        for (auto imageView: m_swapchainImageViews) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }

    void VulkanEngine::recreateSwapchain(uint32_t width, uint32_t height) {
        vkDeviceWaitIdle(m_device);

        destroySwapchain();

        m_windowExtent.width = width;
        m_windowExtent.height = height;

        createSwapchain(m_windowExtent.width, m_windowExtent.height);
    }

    void VulkanEngine::initCommands() {
        auto createInfo =
                VkInit::commandPoolCreateInfo(
                        m_graphicsQueueFamilyIndex,
                        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            MOE_VK_CHECK_MSG(
                    vkCreateCommandPool(m_device, &createInfo, nullptr, &m_frames[i].commandPool),
                    "Failed to create command pool");

            auto allocInfo = VkInit::commandBufferAllocateInfo(m_frames[i].commandPool);
            MOE_VK_CHECK_MSG(
                    vkAllocateCommandBuffers(m_device, &allocInfo, &m_frames[i].mainCommandBuffer),
                    "Failed to allocate command buffer");
        }

        // ImGUI
        MOE_VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_immediateModeCommandPool));

        auto allocInfo = VkInit::commandBufferAllocateInfo(m_immediateModeCommandPool);
        MOE_VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &m_immediateModeCommandBuffer));

        m_mainDeletionQueue.pushFunction([=] {
            vkDestroyCommandPool(m_device, m_immediateModeCommandPool, nullptr);
        });
    }

    void VulkanEngine::initSyncPrimitives() {
        VkFenceCreateInfo fenceInfo = VkInit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
        VkSemaphoreCreateInfo semaphoreInfo = VkInit::semaphoreCreateInfo();

        for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            MOE_VK_CHECK_MSG(
                    vkCreateFence(m_device, &fenceInfo, nullptr, &m_frames[i].inFlightFence),
                    "Failed to create fence");

            MOE_VK_CHECK_MSG(
                    vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_frames[i].imageAvailableSemaphore),
                    "Failed to create semaphore");
        }

        m_perSwapchainImageData.resize(m_swapchainImageCount);
        for (int i = 0; i < m_swapchainImageCount; ++i) {
            MOE_VK_CHECK_MSG(
                    vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_perSwapchainImageData[i].renderFinishedSemaphore),
                    "Failed to create semaphore");
        }

        MOE_VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_immediateModeFence));
        m_mainDeletionQueue.pushFunction([=] {
            vkDestroyFence(m_device, m_immediateModeFence, nullptr);
        });
    }

    void VulkanEngine::initCaches() {
        m_caches.imageCache.init(*this);
        m_caches.meshCache.init(*this);
        m_caches.materialCache.init(*this);

        m_mainDeletionQueue.pushFunction([&] {
            m_caches.materialCache.destroy();
            m_caches.meshCache.destroy();
            m_caches.imageCache.destroy();

            m_caches.fontCache.destroy();
        });
    }

    void VulkanEngine::initDescriptors() {
        Vector<VulkanDescriptorAllocator::PoolSizeRatio> ratios = {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        };

        m_globalDescriptorAllocator.init(m_device, 10, ratios);

        // !
        // todo

        Vector<VulkanDescriptorAllocatorDynamic::PoolSizeRatio> dynamicRatios = {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };

        for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            m_frames[i].descriptorAllocator = {};
            m_frames[i].descriptorAllocator.init(m_device, 10, dynamicRatios);

            // ! note that the index 'i' should not be captured by reference here
            m_mainDeletionQueue.pushFunction([&, i] {
                m_frames[i].descriptorAllocator.destroyPools(m_device);
            });
        }

        m_mainDeletionQueue.pushFunction([&] {
            m_globalDescriptorAllocator.destroyPool(m_device);
        });
    }

    void VulkanEngine::initBindlessSet() {
        m_bindlessSet.init(*this);
        m_mainDeletionQueue.pushFunction([&] {
            m_bindlessSet.destroy();
        });
    }

    void VulkanEngine::initPipelines() {
        m_illuminationBus.init(*this);
        m_renderBus.init(*this);
        m_resourceLoader.init(*this);

        m_pipelines.skinningPipeline.init(*this);
        //m_pipelines.meshPipeline.init(*this);
        //m_pipelines.skyBoxPipeline.init(*this);
        //m_pipelines.shadowMapPipeline.init(*this);
        m_pipelines.csmPipeline.init(*this);
        m_pipelines.gBufferPipeline.init(*this);
        m_pipelines.deferredLightingPipeline.init(*this);
        m_pipelines.spritePipeline.init(*this);
        m_pipelines.fxaaPipeline.init(*this);
        m_pipelines.gammaCorrectionPipeline.init(*this);

        initAndCompilePostFXGraph();

        // init default render target and view
        m_defaultRenderTargetId = m_caches.renderTargetCache.load(VulkanRenderTarget{}).first;
        auto view = VulkanRenderView{};
        view.init(
                this,
                m_defaultRenderTargetId,
                m_defaultCamera.get(),
                Viewport{
                        0,
                        0,
                        m_drawExtent.width,
                        m_drawExtent.height,
                });
        m_defaultRenderViewId =
                m_caches.renderViewCache.load(std::move(view)).first;

        m_pipelines.skyBoxImageId = m_caches.imageCache.loadCubeMapFromFiles(
                {"skybox/right.jpg",
                 "skybox/left.jpg",
                 "skybox/top.jpg",
                 "skybox/bottom.jpg",
                 "skybox/front.jpg",
                 "skybox/back.jpg"},
                VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);
        //m_pipelines.skyBoxPipeline.setSkyBoxImage(m_pipelines.skyBoxImageId);

        m_pipelines.sceneDataBuffer.init(
                *this,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                sizeof(VulkanGPUSceneData),
                FRAMES_IN_FLIGHT);

        m_mainDeletionQueue.pushFunction([&] {
            m_pipelines.sceneDataBuffer.destroy();

            m_pipelines.postFxGraph.destroy();

            m_pipelines.gammaCorrectionPipeline.destroy();
            m_pipelines.fxaaPipeline.destroy();
            m_pipelines.spritePipeline.destroy();
            m_pipelines.deferredLightingPipeline.destroy();
            m_pipelines.gBufferPipeline.destroy();
            m_pipelines.csmPipeline.destroy();
            //m_pipelines.shadowMapPipeline.destroy();
            //m_pipelines.skyBoxPipeline.destroy();
            //m_pipelines.meshPipeline.destroy();
            m_pipelines.skinningPipeline.destroy();

            m_renderBus.destroy();
            m_illuminationBus.destroy();
        });
    }

    void VulkanEngine::initAndCompilePostFXGraph() {
        auto drawExtent3D = VkExtent3D{
                m_drawExtent.width,
                m_drawExtent.height,
                1};
        m_pipelines.postFxGraph.init(*this);
        m_pipelines.postFxGraph.addGlobalInputName("#input", m_drawImageFormat, drawExtent3D);
        m_pipelines.postFxGraph.addStage(
                "fxaa", {"#input"}, m_drawImageFormat,
                drawExtent3D,
                [&](VkCommandBuffer cmdBuffer, Vector<ImageId>& inputs) {
                    m_pipelines.fxaaPipeline.draw(cmdBuffer, inputs[0]);
                });
        m_pipelines.postFxGraph.addStage(
                "gamma_correction", {"fxaa"}, m_swapchainImageFormat,
                drawExtent3D,
                [&](VkCommandBuffer cmdBuffer, Vector<ImageId>& inputs) {
                    m_pipelines.gammaCorrectionPipeline.draw(cmdBuffer, inputs[0]);
                });
        m_pipelines.postFxGraph.setGlobalOutputName("gamma_correction");
        m_pipelines.postFxGraph.setCompilationLogEnabled(true);
        m_pipelines.postFxGraph.compile();

        Logger::info("Post FX Graph Compilation Log:\n{}", m_pipelines.postFxGraph.getCompilationLog());
    }

    void VulkanEngine::queueEvent(WindowEvent event) {
        m_inputBus.pushEvent(event);
    }

    bool VulkanEngine::isKeyPressed(int key) const {
        return glfwGetKey(m_window, key) == GLFW_PRESS;
    }

    void VulkanEngine::resetDynamicState() {
        m_caches.materialCache.resetDynamicState();
        m_renderBus.resetDynamicState();
        m_illuminationBus.resetDynamicState();
    }

}// namespace moe