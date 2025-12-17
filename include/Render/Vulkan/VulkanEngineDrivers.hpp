#pragma once

#include "Core/Meta/Feature.hpp"
#include "Render/Common.hpp"
#include "Render/Vulkan/VulkanLight.hpp"
#include "Render/Vulkan/VulkanRenderable.hpp"
#include "Render/Vulkan/VulkanSprite.hpp"
#include "Render/Vulkan/VulkanSwapBuffer.hpp"
#include "Render/Vulkan/VulkanTypes.hpp"


namespace moe {
    class VulkanEngine;

    struct VulkanLoader {
    public:
        RenderableId load(Loader::GltfT, StringView path);

        ImageId load(Loader::ImageT, StringView path);

        FontId load(Loader::FontT, StringView path, float fontSize, StringView glyphRange);

        void init(VulkanEngine& engine) {
            m_engine = &engine;
        }

        void destroy() {
            m_engine = nullptr;
        }

    private:
        VulkanEngine* m_engine{nullptr};
    };

    struct VulkanIlluminationBus : public Meta::NonCopyable<VulkanIlluminationBus> {
    public:
        enum class RegistryType {
            Static,
            Dynamic
        };

        void init(VulkanEngine& engine);

        void destroy();

        VulkanIlluminationBus& addLight(const VulkanCPULight& light, RegistryType type);

        VulkanIlluminationBus& addStaticLight(const VulkanCPULight& light) {
            return addLight(light, RegistryType::Static);
        }

        VulkanIlluminationBus& addDynamicLight(const VulkanCPULight& light) {
            return addLight(light, RegistryType::Dynamic);
        }

        VulkanIlluminationBus& setAmbient(const glm::vec3& color, float strength) {
            ambientColorStrength = glm::vec4(color, strength);
            return *this;
        }

        VulkanIlluminationBus& setSunlight(const glm::vec3& direction, const glm::vec3& color, float strength);

        VulkanSwapBuffer& getGPUBuffer() { return lightBuffer; }

        VulkanCPULight getSunlight() const { return sunLight; }

        glm::vec4 getAmbientColorStrength() const { return ambientColorStrength; }

        void uploadToGPU(VkCommandBuffer cmdBuffer, uint32_t frameIndex);

        // includes the sun light + static lights + dynamic lights
        size_t getNumLights() const { return staticLights.size() + dynamicLights.size() + 1; }

        void resetDynamicState() { dynamicLights.clear(); }

        void resetAllState() {
            staticLights.clear();
            dynamicLights.clear();
        }

    private:
        VulkanEngine* m_engine{nullptr};
        glm::vec4 ambientColorStrength{1.0f, 1.0f, 1.0f, 1.0f};// rgb color, a strength
        VulkanCPULight sunLight;
        glm::vec3 sunLightDirection;

        Vector<VulkanCPULight> staticLights;
        Vector<VulkanCPULight> dynamicLights;

        VulkanSwapBuffer lightBuffer;
    };

    struct VulkanRenderObjectBus : public Meta::NonCopyable<VulkanRenderObjectBus> {
    public:
        friend class VulkanEngine;

        VulkanRenderObjectBus() = default;
        ~VulkanRenderObjectBus() = default;

        void init(VulkanEngine& engine) {
            m_engine = &engine;
            m_initialized = true;
        }

        void destroy() {
            m_engine = nullptr;
            m_initialized = false;
        }

        VulkanRenderObjectBus& submitRender(RenderableId id, Transform transform) {
            submitRender(RenderCommand{.renderableId = id, .transform = transform, .computeHandle = NULL_COMPUTE_SKIN_HANDLE_ID});
            return *this;
        }

        VulkanRenderObjectBus& submitRender(RenderableId id, Transform transform, ComputeSkinHandleId computeHandle) {
            submitRender(RenderCommand{.renderableId = id, .transform = transform, .computeHandle = computeHandle});
            return *this;
        }

        VulkanRenderObjectBus& submitRender(RenderCommand command) {
            MOE_ASSERT(m_initialized, "VulkanRenderObjectBus not initialized");
            if (m_renderCommands.size() >= MAX_RENDER_COMMANDS) {
                Logger::warn("Render object bus reached max render commands(4096), check if dynamic state is reset properly; exceeding commands will be ignored");
                return *this;
            }
            m_renderCommands.push_back(command);
            return *this;
        }

        VulkanRenderObjectBus& submitSpriteRender(
                const ImageId imageId,
                const Transform& transform,
                const Color& color,
                const glm::vec2& size,
                const glm::vec2& texOffset = glm::vec2(0.0f, 0.0f),
                const glm::vec2& texSize = glm::vec2(0.0f, 0.0f),
                bool isTextSprite = false);

        VulkanRenderObjectBus& submitTextSpriteRender(
                FontId fontId,
                float fontSize,
                StringView text,
                const Transform& transform,
                const Color& color);

        ComputeSkinHandleId submitComputeSkin(ComputeSkinCommand command) {
            MOE_ASSERT(m_initialized, "VulkanRenderObjectBus not initialized");
            if (m_computeSkinCommands.size() >= MAX_RENDER_COMMANDS) {
                Logger::warn("Render object bus reached max render commands(4096), check if dynamic state is reset properly; exceeding commands will be ignored");
                return NULL_COMPUTE_SKIN_HANDLE_ID;
            }
            m_computeSkinCommands.push_back(command);
            return m_nextComputeSkinHandleId++;
        }

        ComputeSkinHandleId submitComputeSkin(RenderableId id, AnimationId animationId, float time) {
            return submitComputeSkin(ComputeSkinCommand{.renderableId = id, .animationId = animationId, .time = time});
        }

        void resetDynamicState() {
            m_renderCommands.clear();
            m_spriteRenderCommands.clear();
            m_computeSkinCommands.clear();
            m_computeSkinHandleToMatrixOffset.clear();
            m_nextComputeSkinHandleId = 0;
        }

        Deque<RenderCommand>& getRenderCommands() { return m_renderCommands; }

        Vector<VulkanSprite>& getSpriteRenderCommands() { return m_spriteRenderCommands; }

        Vector<ComputeSkinCommand>& getComputeSkinCommands() { return m_computeSkinCommands; }

        size_t getNumComputeSkinCommands() const { return m_computeSkinCommands.size(); }

    private:
        static constexpr uint32_t MAX_RENDER_COMMANDS = 4096;
        static constexpr uint32_t MAX_SPRITE_RENDER_COMMANDS = 10240;

        VulkanEngine* m_engine{nullptr};
        bool m_initialized{false};

        Deque<RenderCommand> m_renderCommands;
        Vector<VulkanSprite> m_spriteRenderCommands;

        Vector<ComputeSkinCommand> m_computeSkinCommands;
        Vector<size_t> m_computeSkinHandleToMatrixOffset;
        ComputeSkinHandleId m_nextComputeSkinHandleId{0};

        void setComputeSkinMatrix(ComputeSkinHandleId handle, size_t offset) {
            if (handle >= m_computeSkinHandleToMatrixOffset.size()) {
                m_computeSkinHandleToMatrixOffset.resize(m_computeSkinCommands.size(), INVALID_JOINT_MATRIX_START_INDEX);
            }
            m_computeSkinHandleToMatrixOffset[handle] = offset;
        }

        size_t getComputeSkinMatrix(ComputeSkinHandleId handle) const {
            return m_computeSkinHandleToMatrixOffset.at(handle);
        }
    };
}// namespace moe
