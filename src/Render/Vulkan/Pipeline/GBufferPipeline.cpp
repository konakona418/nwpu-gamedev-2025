#include "Render/Vulkan/Pipeline/GBufferPipeline.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanInitializers.hpp"
#include "Render/Vulkan/VulkanMaterialCache.hpp"
#include "Render/Vulkan/VulkanMeshCache.hpp"
#include "Render/Vulkan/VulkanPipeline.hpp"
#include "Render/Vulkan/VulkanUtils.hpp"

namespace moe {
    namespace Pipeline {
        void GBufferPipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "GBufferPipeline already initialized");

            m_initialized = true;
            m_engine = &engine;

            allocateImages();

            auto vert =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/gbuffer.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/gbuffer.frag.spv");

            auto pushRange = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = sizeof(PushConstants),
            };

            auto pushRanges =
                    Array<VkPushConstantRange, 1>{pushRange};

            VulkanDescriptorLayoutBuilder layoutBuilder{};

            auto descriptorLayouts = Array<VkDescriptorSetLayout, 1>{
                    engine.getBindlessSet().getDescriptorSetLayout(),
            };

            auto pipelineLayoutInfo =
                    VkInit::pipelineLayoutCreateInfo(descriptorLayouts, pushRanges);

            MOE_VK_CHECK(vkCreatePipelineLayout(engine.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

            auto builder = VulkanPipelineBuilder(m_pipelineLayout);
            builder.addShader(vert, frag);
            builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            builder.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.disableBlending();
            builder.enableDepthTesting(true, VK_COMPARE_OP_LESS);
            builder.addColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
            builder.addColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
            builder.addColorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM);
            builder.addColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
            builder.setDepthFormat(VK_FORMAT_D32_SFLOAT);

            if (engine.isMultisamplingEnabled()) {
                Logger::error("Enabling multisampling in deferred rendering is of no use");
                MOE_ASSERT(false, "Enabling multisampling in deferred rendering is of no use");
            } else {
                builder.disableMultisampling();
            }

            m_pipeline = builder.build(engine.m_device);

            vkDestroyShaderModule(engine.m_device, vert, nullptr);
            vkDestroyShaderModule(engine.m_device, frag, nullptr);
        }

        void GBufferPipeline::draw(
                VkCommandBuffer cmdBuffer,
                VulkanMeshCache& meshCache,
                VulkanMaterialCache& materialCache,
                Span<VulkanRenderPacket> drawCommands,
                VulkanAllocatedBuffer& sceneDataBuffer) {
            MOE_ASSERT(m_initialized, "GBufferPipeline not initialized");

            transitionImagesForRendering(cmdBuffer);

            VkClearValue colorClearValue = {.color = {0.0f, 0.0f, 0.0f, 1.0f}};
            VkClearValue depthClearValue = {.depthStencil = {1.0f, 0}};

            auto albedoAttachment =
                    VkInit::renderingAttachmentInfo(
                            gAlbedo.imageView,
                            &colorClearValue,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            auto normalAttachment =
                    VkInit::renderingAttachmentInfo(
                            gNormal.imageView,
                            &colorClearValue,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            auto ormaAttachment =
                    VkInit::renderingAttachmentInfo(
                            gORMA.imageView,
                            &colorClearValue,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            auto emissiveAttachment =
                    VkInit::renderingAttachmentInfo(
                            gEmissive.imageView,
                            &colorClearValue,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            auto depthAttachment = VkInit::renderingAttachmentInfo(
                    gDepth.imageView,
                    &depthClearValue,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            auto colorAttachments = Array<VkRenderingAttachmentInfo, 4>{
                    albedoAttachment,
                    normalAttachment,
                    ormaAttachment,
                    emissiveAttachment,
            };

            auto renderingInfo = VkInit::renderingInfo(m_engine->m_drawExtent, nullptr, &depthAttachment);
            renderingInfo.colorAttachmentCount = colorAttachments.size();
            renderingInfo.pColorAttachments = colorAttachments.data();

            vkCmdBeginRendering(cmdBuffer, &renderingInfo);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            auto bindlessDescriptorSet = m_engine->getBindlessSet().getDescriptorSet();
            vkCmdBindDescriptorSets(
                    cmdBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipelineLayout,
                    0, 1, &bindlessDescriptorSet,
                    0, nullptr);

            for (auto& cmd: drawCommands) {
                auto mesh = meshCache.getMesh(cmd.meshId);
                if (!mesh.has_value()) {
                    Logger::warn("Invalid mesh id {}, skipping draw command", cmd.meshId);
                    continue;
                }

                auto& meshAsset = mesh.value();
                MOE_ASSERT(sceneDataBuffer.address != 0, "Invalid scene data buffer");

                const auto viewport = VkViewport{
                        .x = 0,
                        .y = 0,
                        .width = (float) m_engine->m_drawExtent.width,
                        .height = (float) m_engine->m_drawExtent.height,
                        .minDepth = 0.f,
                        .maxDepth = 1.f,
                };
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

                VkRect2D scissor = {.offset = {0, 0}, .extent = m_engine->m_drawExtent};
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                vkCmdBindIndexBuffer(cmdBuffer, meshAsset.gpuBuffer.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                auto vertexBufferAddr =
                        cmd.skinned
                                ? cmd.skinnedVertexBufferAddr
                                : meshAsset.gpuBuffer.vertexBufferAddr;

                const auto pushConstants = PushConstants{
                        .transform = cmd.transform,
                        .clampedInverseTransform =
                                glm::mat3(glm::inverse(cmd.transform)),
                        .vertexBufferAddr = vertexBufferAddr,
                        .sceneDataAddress = sceneDataBuffer.address,
                        .materialId = cmd.materialId,
                };

                vkCmdPushConstants(
                        cmdBuffer,
                        m_pipelineLayout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(PushConstants),
                        &pushConstants);

                vkCmdDrawIndexed(cmdBuffer, meshAsset.gpuBuffer.indexCount, 1, 0, 0, 0);
            }

            vkCmdEndRendering(cmdBuffer);

            transitionImagesForSampling(cmdBuffer);
        }

        void GBufferPipeline::destroy() {
            MOE_ASSERT(m_initialized, "GBufferPipeline not initialized");

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);

            m_initialized = false;
            m_engine = nullptr;
        }

        void GBufferPipeline::allocateImages() {
            auto extent = VkExtent3D{
                    .width = m_engine->m_drawExtent.width,
                    .height = m_engine->m_drawExtent.height,
                    .depth = 1,
            };

            auto depth = m_engine->allocateImage(
                    extent,
                    VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT);

            auto normal = m_engine->allocateImage(
                    extent,
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT);

            auto albedo = m_engine->allocateImage(
                    extent,
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT);

            auto orma = m_engine->allocateImage(
                    extent,
                    VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT);

            auto emissive = m_engine->allocateImage(
                    extent,
                    VK_FORMAT_R16G16B16A16_SFLOAT,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_SAMPLED_BIT);

            auto& imageCache = m_engine->m_caches.imageCache;

            gDepthId = imageCache.addImage(std::move(depth));
            gNormalId = imageCache.addImage(std::move(normal));
            gAlbedoId = imageCache.addImage(std::move(albedo));
            gORMAId = imageCache.addImage(std::move(orma));
            gEmissiveId = imageCache.addImage(std::move(emissive));

            gDepth = imageCache.getImage(gDepthId).value();
            gNormal = imageCache.getImage(gNormalId).value();
            gAlbedo = imageCache.getImage(gAlbedoId).value();
            gORMA = imageCache.getImage(gORMAId).value();
            gEmissive = imageCache.getImage(gEmissiveId).value();
        }

        void GBufferPipeline::transitionImagesForRendering(VkCommandBuffer cmdBuffer) {
            VkUtils::transitionImage(
                    cmdBuffer,
                    gDepth.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gAlbedo.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gNormal.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gORMA.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gEmissive.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        void GBufferPipeline::transitionImagesForSampling(VkCommandBuffer cmdBuffer) {
            VkUtils::transitionImage(
                    cmdBuffer,
                    gDepth.image,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gAlbedo.image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gNormal.image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gORMA.image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            VkUtils::transitionImage(
                    cmdBuffer,
                    gEmissive.image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }// namespace Pipeline
}// namespace moe