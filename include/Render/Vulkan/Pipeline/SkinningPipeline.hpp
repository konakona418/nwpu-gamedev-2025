#pragma once

#include "Render/Common.hpp"
#include "Render/Vulkan/VulkanIdTypes.hpp"
#include "Render/Vulkan/VulkanRenderable.hpp"
#include "Render/Vulkan/VulkanTypes.hpp"

namespace moe {
    class VulkanEngine;
    class VulkanMeshCache;
}// namespace moe

namespace moe {
    namespace Pipeline {
        struct SkinningPipeline {
        public:
            SkinningPipeline() = default;
            ~SkinningPipeline() = default;

            void init(VulkanEngine& engine);

            void beginFrame(size_t frameIndex);

            size_t appendJointMatrices(Span<glm::mat4> jointMatrices, size_t frameIndex);

            void compute(
                    VkCommandBuffer cmdBuffer,
                    VulkanMeshCache& meshCache,
                    Span<VulkanRenderPacket> drawCommands,
                    size_t frameIndex);

            void destroy();

            // this should be called by VulkanEngine before using the pipeline each frame
            void resetDynamicVertexBuffer(size_t frameIndex);

        private:
            static constexpr uint32_t MAX_JOINT_MATRIX_COUNT = 10240;       // max 10k joint matrices
            static constexpr uint32_t MIN_DYNAMIC_VERTEX_BUFFER_SIZE = 1024;// 1024 vertices
            static constexpr uint32_t DYNAMIC_VERTEX_BUFFER_GROWTH_FACTOR = 2;

            struct PushConstants {
                VkDeviceAddress vertexBufferAddr;
                VkDeviceAddress skinningDataBufferAddr;
                VkDeviceAddress jointMatrixBufferAddr;
                VkDeviceAddress outputVertexBufferAddr;
                uint32_t jointMatrixStartIndex;
                uint32_t vertexCount;
            };

            struct DynamicVertexBuffer {
                size_t bufferIdx;
                VkDeviceSize size;
                VkDeviceSize capacity;

                VulkanAllocatedBuffer buffer{};
                VkDeviceAddress bufferAddr{};
            };

            void ensureDynamicVertexBufferSize(size_t requiredVertexCount, size_t frameIndex);

            struct SwapData {
                VulkanAllocatedBuffer jointMatrixBuffer;
                size_t jointMatrixBufferSize;

                DynamicVertexBuffer dynamicVertexBuffer;
            };

            Array<SwapData, Constants::FRAMES_IN_FLIGHT> m_swapData;

            VulkanEngine* m_engine{nullptr};
            bool m_initialized{false};

            VkPipelineLayout m_pipelineLayout;
            VkPipeline m_pipeline;
        };
    }// namespace Pipeline
}// namespace moe