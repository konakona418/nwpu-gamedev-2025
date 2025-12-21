#pragma once

#include "Render/Vulkan/Pipeline/CSMPipeline.hpp"
#include "Render/Vulkan/Pipeline/DeferredLightingPipeline.hpp"
#include "Render/Vulkan/Pipeline/GBufferPipeline.hpp"
//#include "Render/Vulkan/Pipeline/MeshPipeline.hpp"
#include "Render/Vulkan/Pipeline/PostFXPipeline.hpp"
#include "Render/Vulkan/Pipeline/ShadowMapPipeline.hpp"
#include "Render/Vulkan/Pipeline/SkinningPipeline.hpp"
#include "Render/Vulkan/Pipeline/SkyBoxPipeline.hpp"
#include "Render/Vulkan/Pipeline/SpritePipeline.hpp"

#include "Render/Vulkan/VulkanAnimationCache.hpp"
#include "Render/Vulkan/VulkanBindlessSet.hpp"
#include "Render/Vulkan/VulkanCamera.hpp"
#include "Render/Vulkan/VulkanDescriptors.hpp"
#include "Render/Vulkan/VulkanEngineDrivers.hpp"
#include "Render/Vulkan/VulkanFont.hpp"
#include "Render/Vulkan/VulkanIm3dDriver.hpp"
#include "Render/Vulkan/VulkanImageCache.hpp"
#include "Render/Vulkan/VulkanMaterialCache.hpp"
#include "Render/Vulkan/VulkanMeshCache.hpp"
#include "Render/Vulkan/VulkanObjectCache.hpp"
#include "Render/Vulkan/VulkanPostFXGraph.hpp"
#include "Render/Vulkan/VulkanRenderTarget.hpp"
#include "Render/Vulkan/VulkanScene.hpp"
#include "Render/Vulkan/VulkanSwapBuffer.hpp"
#include "Render/Vulkan/VulkanTypes.hpp"


#include "Core/Input.hpp"


#include <GLFW/glfw3.h>

namespace moe {
    struct VulkanGPUMeshBuffer;

    struct DeletionQueue {
        Deque<Function<void()>> deletors;

        void pushFunction(Function<void()>&& function);

        void flush();
    };

    struct FrameData {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;

        VkSemaphore imageAvailableSemaphore;

        VkFence inFlightFence;

        DeletionQueue deletionQueue;
        VulkanDescriptorAllocatorDynamic descriptorAllocator;
    };

    struct PerSwapchainImageData {
        VkSemaphore renderFinishedSemaphore;
    };

    constexpr uint32_t FRAMES_IN_FLIGHT = Constants::FRAMES_IN_FLIGHT;

    struct VulkanEngineInitializers {
        int viewportWidth{1280};
        int viewportHeight{720};

        float fovDeg{45.0f};

        glm::vec3 csmCameraScale{3.0f, 3.0f, 3.0f};
    };

    class VulkanEngine {
    public:
        DeletionQueue m_mainDeletionQueue;
        VmaAllocator m_allocator;
        VulkanDescriptorAllocator m_globalDescriptorAllocator;

        bool m_isInitialized{false};
        int32_t m_frameNumber{0};
        bool m_stopRendering{false};
        bool m_resizeRequested{false};
        VkExtent2D m_windowExtent;

        VkSampleCountFlagBits m_msaaSamples{VK_SAMPLE_COUNT_1_BIT};
        // ! msaa x4; disable this if deferred rendering is implemented; use fxaa then
        bool m_enableFxaa{true};

        glm::vec3 m_shadowMapCameraScale{3.0f, 3.0f, 3.0f};

        GLFWwindow* m_window{nullptr};
        std::pair<float, float> m_lastMousePos{0.0f, 0.0f};
        bool m_firstMouse{true};

        VkInstance m_instance;
        VkDebugUtilsMessengerEXT m_debugMessenger;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;
        VkSurfaceKHR m_surface;

        VkSwapchainKHR m_swapchain;
        VkFormat m_swapchainImageFormat;
        Vector<VkImage> m_swapchainImages;
        Vector<VkImageView> m_swapchainImageViews;
        VkExtent2D m_swapchainExtent;
        uint32_t m_swapchainImageCount;

        Array<FrameData, FRAMES_IN_FLIGHT> m_frames;
        Vector<PerSwapchainImageData> m_perSwapchainImageData;

        VkQueue m_graphicsQueue;
        uint32_t m_graphicsQueueFamilyIndex;

        VkExtent2D m_drawExtent;
        VkFormat m_drawImageFormat{VK_FORMAT_R16G16B16A16_SFLOAT};
        VkFormat m_depthImageFormat{VK_FORMAT_D32_SFLOAT};

        VkFence m_immediateModeFence;
        VkCommandBuffer m_immediateModeCommandBuffer;
        VkCommandPool m_immediateModeCommandPool;

        VulkanBindlessSet m_bindlessSet;

        InputBus m_inputBus{};
        VulkanIlluminationBus m_illuminationBus{};
        VulkanRenderObjectBus m_renderBus{};
        VulkanLoader m_resourceLoader{};

        Queue<Function<void()>> m_imguiDrawQueue;
        Queue<Function<void()>> m_im3dDrawQueue;

        RenderTargetId m_defaultRenderTargetId{NULL_RENDER_TARGET_ID};
        RenderViewId m_defaultRenderViewId{NULL_RENDER_VIEW_ID};

        RenderTargetId m_defaultSpriteRenderTargetId{NULL_RENDER_TARGET_ID};
        RenderViewId m_defaultSpriteRenderViewId{NULL_RENDER_VIEW_ID};

        Pinned<VulkanCamera> m_defaultCamera{nullptr};

        Pinned<Vulkan2DCamera> m_defaultSpriteCamera{nullptr};

        struct {
            VulkanImageCache imageCache;
            VulkanMeshCache meshCache;
            VulkanMaterialCache materialCache;
            VulkanObjectCache objectCache;
            VulkanAnimationCache animationCache;

            VulkanRenderTargetCache renderTargetCache{};
            VulkanRenderViewCache renderViewCache{};
            VulkanFontCache fontCache{};
        } m_caches;

        struct {
            Pipeline::SkinningPipeline skinningPipeline;
            //Pipeline::VulkanMeshPipeline meshPipeline;
            //Pipeline::SkyBoxPipeline skyBoxPipeline;
            //Pipeline::ShadowMapPipeline shadowMapPipeline;
            Pipeline::CSMPipeline csmPipeline;
            Pipeline::GBufferPipeline gBufferPipeline;
            Pipeline::SpritePipeline spritePipeline;
            Pipeline::DeferredLightingPipeline deferredLightingPipeline;
            Pipeline::FXAAPipeline fxaaPipeline;
            Pipeline::BlendTwoPipeline blendTwoPipeline;
            Pipeline::GammaCorrectionPipeline gammaCorrectionPipeline;

            VulkanPostFXGraph postFxGraph;

            ImageId skyBoxImageId{NULL_IMAGE_ID};
            VulkanSwapBuffer sceneDataBuffer;
        } m_pipelines;

        VulkanIm3dDriver m_im3dDriver;

        static VulkanEngine& get();

        void init(const VulkanEngineInitializers& initializers = {});

        void cleanup();

        void draw();

        void run();

        void beginFrame();

        void endFrame();

        void addImGuiDrawCommand(Function<void()>&& fn) {
            if (m_stopRendering) {
                return;
            }
            m_imguiDrawQueue.push(std::move(fn));
        }

        void addIm3dDrawCommand(Function<void()>&& fn) {
            if (m_stopRendering) {
                return;
            }
            m_im3dDrawQueue.push(std::move(fn));
        }

        VulkanBindlessSet& getBindlessSet() {
            MOE_ASSERT(m_bindlessSet.isInitialized(), "VulkanBindlessSet not initialized");
            return m_bindlessSet;
        }

        bool isFxaaEnabled() const { return m_enableFxaa; }

        void setFxaaEnabled(bool enabled) { m_enableFxaa = enabled; }

        void immediateSubmit(Function<void(VkCommandBuffer)>&& fn, Function<void()>&& postFn = nullptr);

        VulkanAllocatedBuffer allocateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

        VulkanAllocatedImage allocateImage(VkImageCreateInfo imageInfo);

        VulkanAllocatedImage allocateImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap = false);

        VulkanAllocatedImage allocateImage(void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap = false);

        VulkanAllocatedImage allocateCubeMapImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap = false);

        VulkanAllocatedImage allocateCubeMapImage(Array<void*, 6> data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap = false);

        Optional<VulkanAllocatedImage> loadImageFromFile(StringView filename, VkFormat format, VkImageUsageFlags usage, bool mipmap = false);

        Optional<VulkanAllocatedImage> loadImageFromMemory(Span<uint8_t> imageData, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, bool mipmap = false);

        void destroyImage(VulkanAllocatedImage& image);

        void destroyBuffer(VulkanAllocatedBuffer& buffer);

        VulkanGPUMeshBuffer uploadMesh(Span<uint32_t> indices, Span<Vertex> vertices, Span<SkinningData> skinningData = {});

        FrameData& getCurrentFrame() { return m_frames[m_frameNumber % FRAMES_IN_FLIGHT]; }

        size_t getCurrentFrameIndex() const { return m_frameNumber % FRAMES_IN_FLIGHT; }

        VulkanCamera& getDefaultCamera() { return *m_defaultCamera; }

        Vulkan2DCamera& getDefaultSpriteCamera() { return *m_defaultSpriteCamera; }

        InputBus& getInputBus() { return m_inputBus; }

        VulkanLoader& getResourceLoader() { return m_resourceLoader; }

        template<typename T>
        T& getBus() {
            if constexpr (std::is_same_v<T, InputBus>) {
                return m_inputBus;
            } else if constexpr (std::is_same_v<T, VulkanIlluminationBus>) {
                return m_illuminationBus;
            } else if constexpr (std::is_same_v<T, VulkanRenderObjectBus>) {
                return m_renderBus;
            } else {
                static_assert(std::false_type::value, "Unsupported bus type");
            }
        }

        bool isMultisamplingEnabled() const { return m_msaaSamples != VK_SAMPLE_COUNT_1_BIT; }

        Pair<uint32_t, uint32_t> getCanvasSize() const {
            return {m_drawExtent.width, m_drawExtent.height};
        }

    private:
        void initDefaults(const VulkanEngineInitializers& initializers);

        void initWindow();

        void queueEvent(WindowEvent event);

        void initVulkanInstance();

        void initImGUI();

        void initIm3d();

        void initSwapchain();

        void createSwapchain(uint32_t width, uint32_t height);

        void destroySwapchain();

        void recreateSwapchain(uint32_t width, uint32_t height);

        void initCommands();

        void initSyncPrimitives();

        void initDescriptors();

        void initCaches();

        void initBindlessSet();

        void initPipelines();

        void initAndCompilePostFXGraph();

        void initRenderViewsAndTargets();

        void drawImGUI(VkCommandBuffer commandBuffer, VkImageView drawTarget);

        bool isKeyPressed(int key) const;

        void resetDynamicState();
    };

}// namespace moe