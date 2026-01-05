#include "Render/Vulkan/Pipeline/ShadowMapPipeline.hpp"
#include "Render/Vulkan/VulkanCamera.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanInitializers.hpp"
#include "Render/Vulkan/VulkanMaterialCache.hpp"
#include "Render/Vulkan/VulkanMeshCache.hpp"
#include "Render/Vulkan/VulkanPipeline.hpp"
#include "Render/Vulkan/VulkanRenderable.hpp"
#include "Render/Vulkan/VulkanUtils.hpp"


namespace moe {
    namespace Pipeline {
        void ShadowMapPipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "ShadowMapPipeline is already initialized");

            m_engine = &engine;
            m_initialized = true;

            auto vert =
                    VkUtils::createShaderModuleFromFile(
                            m_engine->m_device, "shaders/shadowmap_depth.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(
                            m_engine->m_device, "shaders/shadowmap_depth.frag.spv");

            auto pushRange = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = sizeof(PushConstants),
            };

            auto ranges = Array<VkPushConstantRange, 1>{pushRange};
            auto descriptorLayouts = Array<VkDescriptorSetLayout, 1>{
                    engine.getBindlessSet().getDescriptorSetLayout(),
            };

            auto pipelineLayout =
                    VkInit::pipelineLayoutCreateInfo(descriptorLayouts, ranges);
            MOE_VK_CHECK(vkCreatePipelineLayout(engine.m_device, &pipelineLayout, nullptr, &m_pipelineLayout));

            auto builder = VulkanPipelineBuilder(m_pipelineLayout);
            builder.addShader(vert, frag);
            builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            // ! use front face culling for shadow map
            builder.setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.disableMultisampling();
            builder.disableBlending();
            builder.enableDepthTesting(true, VK_COMPARE_OP_LESS);
            builder.setDepthFormat(VK_FORMAT_D32_SFLOAT);
            builder.build(engine.m_device);

            m_pipeline = builder.build(engine.m_device);

            auto shadowMapImageInfo =
                    VkInit::imageCreateInfo(
                            VK_FORMAT_D32_SFLOAT,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            {m_shadowMapSize, m_shadowMapSize, 1});

            auto image = engine.allocateImage(shadowMapImageInfo);
            m_shadowMapImageId = engine.m_caches.imageCache.addImage(std::move(image));

            auto shadowMapImage = engine.m_caches.imageCache.getImage(m_shadowMapImageId).value();

            m_shadowMapImageView = shadowMapImage.imageView;

            vkDestroyShaderModule(engine.m_device, vert, nullptr);
            vkDestroyShaderModule(engine.m_device, frag, nullptr);
        }

        void ShadowMapPipeline::draw(
                VkCommandBuffer cmdBuffer,
                VulkanMeshCache& meshCache,
                Span<VulkanRenderPacket> drawCommands,
                const VulkanCamera& camera,
                glm::vec3 lightDir) {
            MOE_ASSERT(m_initialized, "ShadowMapPipeline is not initialized");

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
            auto bindlessDescriptorSet = m_engine->getBindlessSet().getDescriptorSet();
            vkCmdBindDescriptorSets(
                    cmdBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipelineLayout,
                    0, 1, &bindlessDescriptorSet,
                    0, nullptr);

            auto shadowMapResult = m_engine->m_caches.imageCache.getImage(m_shadowMapImageId);
            MOE_ASSERT(shadowMapResult.has_value(), "Invalid CSM shadow map image id");
            auto shadowMap = shadowMapResult.value();

            VkUtils::transitionImage(
                    cmdBuffer,
                    shadowMap.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            float aspect = (float) m_engine->m_drawExtent.width / (float) m_engine->m_drawExtent.height;
            auto corners = camera.getFrustumCornersWorldSpace(aspect);
            auto csmCam = VulkanCamera::getCSMCamera(corners, lightDir, m_shadowMapSize);
            m_shadowMapLightTransform = csmCam.viewProj;

            auto depthClearValue = VkClearValue{.depthStencil = {1.0f, 0}};
            auto depthAttachment = VkInit::renderingAttachmentInfo(
                    m_shadowMapImageView,
                    &depthClearValue,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            auto renderingInfo = VkInit::renderingInfo(
                    VkExtent2D{m_shadowMapSize, m_shadowMapSize},
                    nullptr,
                    &depthAttachment);

            vkCmdBeginRendering(cmdBuffer, &renderingInfo);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            auto bindlessSet = m_engine->getBindlessSet().getDescriptorSet();
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &bindlessSet, 0, nullptr);

            const auto viewport = VkViewport{
                    .x = 0,
                    .y = 0,
                    .width = (float) m_shadowMapSize,
                    .height = (float) m_shadowMapSize,
                    .minDepth = 0.f,
                    .maxDepth = 1.f,
            };
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            const auto scissor = VkRect2D{
                    .offset = {},
                    .extent = {m_shadowMapSize, m_shadowMapSize},
            };
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            for (auto& drawCommand: drawCommands) {
                auto mesh = m_engine->m_caches.meshCache.getMesh(drawCommand.meshId).value();
                vkCmdBindIndexBuffer(cmdBuffer, mesh.gpuBuffer.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                auto pushConstants = PushConstants{
                        .mvp = m_shadowMapLightTransform * drawCommand.transform,
                        .vertexBufferAddr =
                                drawCommand.skinned
                                        ? drawCommand.skinnedVertexBufferAddr
                                        : mesh.gpuBuffer.vertexBufferAddr,
                };

                vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
                vkCmdDrawIndexed(cmdBuffer, mesh.gpuBuffer.indexCount, 1, 0, 0, 0);
            }


            vkCmdEndRendering(cmdBuffer);

            VkUtils::transitionImage(
                    cmdBuffer,
                    shadowMap.image,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        void ShadowMapPipeline::destroy() {
            MOE_ASSERT(m_initialized, "ShadowMapPipeline is not initialized");

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);

            m_engine = nullptr;
            m_initialized = false;
        }
    }// namespace Pipeline
}// namespace moe