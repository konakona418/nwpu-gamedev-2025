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

        private:
            static constexpr uint32_t MAX_JOINT_MATRIX_COUNT = 10240;// max 10k joint matrices

            struct PushConstants {
                VkDeviceAddress vertexBufferAddr;
                VkDeviceAddress skinningDataBufferAddr;
                VkDeviceAddress jointMatrixBufferAddr;
                VkDeviceAddress outputVertexBufferAddr;
                uint32_t jointMatrixStartIndex;
                uint32_t vertexCount;
            };

            struct SwapData {
                VulkanAllocatedBuffer jointMatrixBuffer;
                size_t jointMatrixBufferSize;
            };

            Array<SwapData, Constants::FRAMES_IN_FLIGHT> m_swapData;

            VulkanEngine* m_engine{nullptr};
            bool m_initialized{false};

            VkPipelineLayout m_pipelineLayout;
            VkPipeline m_pipeline;
        };
    }// namespace Pipeline
}// namespace moe