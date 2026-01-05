#include "Render/Vulkan/Pipeline/SkinningPipeline.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanInitializers.hpp"
#include "Render/Vulkan/VulkanMeshCache.hpp"
#include "Render/Vulkan/VulkanPipeline.hpp"
#include "Render/Vulkan/VulkanRenderable.hpp"
#include "Render/Vulkan/VulkanUtils.hpp"

namespace moe {
    namespace Pipeline {
        void SkinningPipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "SkyBoxPipeline already initialized");

            m_engine = &engine;
            auto shader = VkUtils::createShaderModuleFromFile(m_engine->m_device, "shaders/skinning.comp.spv");

            auto pushRange = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                    .offset = 0,
                    .size = sizeof(PushConstants),
            };

            auto descriptorSetLayout = engine.m_bindlessSet.getDescriptorSetLayout();

            auto pushRanges = Array<VkPushConstantRange, 1>{pushRange};
            auto descriptorLayouts = Array<VkDescriptorSetLayout, 1>{descriptorSetLayout};

            auto pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo(descriptorLayouts, pushRanges);
            MOE_VK_CHECK(vkCreatePipelineLayout(engine.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

            auto builder = VulkanComputePipelineBuilder{m_pipelineLayout};
            builder.setShader(shader);
            m_pipeline = builder.build(m_engine->m_device);

            vkDestroyShaderModule(engine.m_device, shader, nullptr);

            m_initialized = true;

            for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
                m_swapData[i].jointMatrixBuffer =
                        engine.allocateBuffer(
                                sizeof(glm::mat4) * MAX_JOINT_MATRIX_COUNT,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                VMA_MEMORY_USAGE_AUTO);
                m_swapData[i].jointMatrixBufferSize = 0;

                // initialize dynamic vertex buffer
                m_swapData[i].dynamicVertexBuffer.bufferIdx = i;
                m_swapData[i].dynamicVertexBuffer.size = 0;
                m_swapData[i].dynamicVertexBuffer.capacity = 0;
                ensureDynamicVertexBufferSize(MIN_DYNAMIC_VERTEX_BUFFER_SIZE, i);
            }
        }

        void SkinningPipeline::destroy() {
            MOE_ASSERT(m_initialized, "SkyBoxPipeline not initialized");

            for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
                m_engine->destroyBuffer(m_swapData[i].jointMatrixBuffer);
                m_engine->destroyBuffer(m_swapData[i].dynamicVertexBuffer.buffer);
            }

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);
        }

        void SkinningPipeline::beginFrame(size_t frameIndex) {
            MOE_ASSERT(m_initialized, "SkinningPipeline not initialized");
            MOE_ASSERT(frameIndex < FRAMES_IN_FLIGHT, "Invalid frame index");

            m_swapData[frameIndex].jointMatrixBufferSize = 0;
        }

        size_t SkinningPipeline::appendJointMatrices(Span<glm::mat4> jointMatrices, size_t frameIndex) {
            MOE_ASSERT(m_initialized, "SkinningPipeline is not initialized");
            MOE_ASSERT(frameIndex < FRAMES_IN_FLIGHT, "Invalid frame index");

            auto& swapData = m_swapData[frameIndex];

            size_t before = swapData.jointMatrixBufferSize;

            size_t jointMatrixCount = jointMatrices.size();
            size_t jointMatrixSize = jointMatrixCount * sizeof(glm::mat4);

            if (before + jointMatrixCount > MAX_JOINT_MATRIX_COUNT) {
                Logger::error("Exceeded maximum joint matrix count {}, clamping to max", MAX_JOINT_MATRIX_COUNT);
                return 0;
            }

            std::memcpy(
                    reinterpret_cast<glm::mat4*>(swapData.jointMatrixBuffer.vmaAllocationInfo.pMappedData) + before,
                    jointMatrices.data(),
                    jointMatrixSize);
            swapData.jointMatrixBufferSize += jointMatrixCount;

            return before;
        }

        void SkinningPipeline::compute(
                VkCommandBuffer cmdBuffer,
                VulkanMeshCache& meshCache,
                Span<VulkanRenderPacket> drawCommands,
                size_t frameIndex) {
            MOE_ASSERT(m_initialized, "SkinningPipeline is not initialized");

            auto& swapData = m_swapData[frameIndex];
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

            VkDeviceSize requiredDynamicVertexBufferSize = 0;
            // required size
            for (auto& packet: drawCommands) {
                if (packet.skinned) {
                    auto mesh = meshCache.getMesh(packet.meshId).value();
                    requiredDynamicVertexBufferSize += mesh.gpuBuffer.vertexCount;
                }
            }

            // ensure dynamic vertex buffer is large enough for this frame
            // note that we do this before processing the draw commands, to avoid providing invalid addresses to the GPU
            ensureDynamicVertexBufferSize(requiredDynamicVertexBufferSize, frameIndex);

            for (auto& packet: drawCommands) {
                if (!packet.skinned) {
                    packet.skinnedVertexBufferAddr = 0;
                    continue;
                }

                if (packet.jointMatrixStartIndex == INVALID_JOINT_MATRIX_START_INDEX) {
                    Logger::warn("Skinned mesh draw command has invalid joint matrix start index, skipping. Note that the matrices must be uploaded before calling this function");
                    packet.skinnedVertexBufferAddr = 0;
                    continue;
                }

                auto mesh = meshCache.getMesh(packet.meshId).value();

                auto& dynamicVertexBuffer = swapData.dynamicVertexBuffer;
                VkDeviceSize offset = dynamicVertexBuffer.size * sizeof(Vertex);// current offset in bytes
                dynamicVertexBuffer.size += mesh.gpuBuffer.vertexCount;         // increase size

                // ! dynamicVertexBuffer.bufferAddr is the base address of the dynamic vertex buffer,
                // ! may be different each frame due to resizing
                VkDeviceAddress skinnedBufferAddress = dynamicVertexBuffer.bufferAddr + offset;// address of the skinned vertex buffer

                // store the skinned vertex buffer address in the render packet for later use
                packet.skinnedVertexBufferAddr = skinnedBufferAddress;

                auto pushConstants = PushConstants{
                        .vertexBufferAddr = mesh.gpuBuffer.vertexBufferAddr,
                        .skinningDataBufferAddr = mesh.gpuBuffer.skinningDataBufferAddr,
                        .jointMatrixBufferAddr = swapData.jointMatrixBuffer.address,
                        .outputVertexBufferAddr = skinnedBufferAddress,
                        .jointMatrixStartIndex = (uint32_t) packet.jointMatrixStartIndex,
                        .vertexCount = mesh.gpuBuffer.vertexCount,
                };

                vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pushConstants);

                constexpr uint32_t workGroupSize = 128;
                const auto groupCount = (uint32_t) std::ceil(mesh.gpuBuffer.vertexCount / (float) workGroupSize);
                vkCmdDispatch(cmdBuffer, groupCount, 1, 1);
            }
        }

        void SkinningPipeline::ensureDynamicVertexBufferSize(size_t requiredVertexCount, size_t frameIndex) {
            MOE_ASSERT(m_initialized, "SkinningPipeline not initialized");

            auto& swapData = m_swapData[frameIndex];
            if (swapData.dynamicVertexBuffer.capacity >= requiredVertexCount) {
                return;
            }

            auto newCapacity = swapData.dynamicVertexBuffer.capacity;
            if (newCapacity == 0) {
                newCapacity = MIN_DYNAMIC_VERTEX_BUFFER_SIZE;
            }

            while (newCapacity < requiredVertexCount) {
                newCapacity *= DYNAMIC_VERTEX_BUFFER_GROWTH_FACTOR;
            }

            if (swapData.dynamicVertexBuffer.buffer.buffer != VK_NULL_HANDLE) {
                m_engine->destroyBuffer(swapData.dynamicVertexBuffer.buffer);
            }

            swapData.dynamicVertexBuffer.buffer = m_engine->allocateBuffer(
                    newCapacity * sizeof(Vertex),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY);

            swapData.dynamicVertexBuffer.bufferAddr = swapData.dynamicVertexBuffer.buffer.address;
            swapData.dynamicVertexBuffer.capacity = newCapacity;

            moe::Logger::info("Resized skinning dynamic vertex buffer to {} vertices, frame index {}", newCapacity, frameIndex);
        }

        void SkinningPipeline::resetDynamicVertexBuffer(size_t frameIndex) {
            MOE_ASSERT(m_initialized, "SkinningPipeline not initialized");

            auto& swapData = m_swapData[frameIndex];
            swapData.dynamicVertexBuffer.size = 0;
        }
    }// namespace Pipeline
}// namespace moe