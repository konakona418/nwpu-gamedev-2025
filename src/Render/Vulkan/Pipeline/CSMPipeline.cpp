#include "Render/Vulkan/Pipeline/CSMPipeline.hpp"
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
        void CSMPipeline::init(VulkanEngine& engine, Array<float, SHADOW_CASCADE_COUNT> cascadeSplitRatios) {
            MOE_ASSERT(!m_initialized, "CSMPipeline is already initialized");

            m_engine = &engine;
            // Ensure cascadeSplitRatios are monotonic and normalized (values between 0 and 1, increasing)
            for (size_t i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
                MOE_ASSERT(cascadeSplitRatios[i] > 0.0f && cascadeSplitRatios[i] <= 1.0f, "cascadeSplitRatios must be in (0, 1]");
                if (i > 0) {
                    MOE_ASSERT(cascadeSplitRatios[i] > cascadeSplitRatios[i - 1], "cascadeSplitRatios must be strictly increasing");
                }
            }
            m_cascadeSplitRatios = cascadeSplitRatios;
            m_initialized = true;

            auto vert =
                    VkUtils::createShaderModuleFromFile(
                            m_engine->m_device, "shaders/csm_depth.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(
                            m_engine->m_device, "shaders/csm_depth.frag.spv");

            auto pushRange = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
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

            auto builder =
                    VulkanPipelineBuilder(m_pipelineLayout);
            builder.addShader(vert, frag);
            builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            // ! use front face culling for shadow map
            builder.setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.disableMultisampling();
            builder.disableBlending();
            builder.enableDepthTesting(true, VK_COMPARE_OP_LESS);
            builder.setDepthFormat(VK_FORMAT_D32_SFLOAT);

            m_pipeline = builder.build(engine.m_device);

            auto csmImageInfo =
                    VkInit::imageCreateInfo(
                            VK_FORMAT_D32_SFLOAT,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            {m_csmShadowMapSize, m_csmShadowMapSize, 1});

            csmImageInfo.mipLevels = 1;
            csmImageInfo.arrayLayers = SHADOW_CASCADE_COUNT;

            auto image = engine.allocateImage(csmImageInfo);
            m_shadowMapImageId = engine.m_caches.imageCache.addImage(std::move(image));
            // ! fixme: this will add a independent 1-layer image view to the descriptor set,
            // ! and the id is mapped to **THAT** image view,
            // ! instead of the image views array we create below
            // ! holy shit
            // ! check if VK_IMAGE_VIEW_TYPE_2D_ARRAY works
            // ! another way: use individual image views for cascade rendering, while creating a 2D_ARRAY view for sampling

            auto csmImageRef = engine.m_caches.imageCache.getImage(m_shadowMapImageId).value();

            for (uint32_t i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
                VkImageViewCreateInfo viewInfo = VkInit::imageViewCreateInfo(
                        VK_FORMAT_D32_SFLOAT,
                        csmImageRef.image,
                        VK_IMAGE_ASPECT_DEPTH_BIT);
                viewInfo.subresourceRange.baseArrayLayer = i;
                viewInfo.subresourceRange.layerCount = 1;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseMipLevel = 0;

                VkImageView imageView;
                MOE_VK_CHECK(vkCreateImageView(engine.m_device, &viewInfo, nullptr, &imageView));
                m_shadowMapImageViews[i] = imageView;
            }

            vkDestroyShaderModule(engine.m_device, vert, nullptr);
            vkDestroyShaderModule(engine.m_device, frag, nullptr);
        }

        void CSMPipeline::draw(
                VkCommandBuffer cmdBuffer,
                VulkanMeshCache& meshCache,
                Span<VulkanRenderPacket> drawCommands,
                const VulkanCamera& camera,
                glm::vec3 lightDir) {
            MOE_ASSERT(m_initialized, "CSMPipeline is not initialized");

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
            auto bindlessDescriptorSet = m_engine->getBindlessSet().getDescriptorSet();
            vkCmdBindDescriptorSets(
                    cmdBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipelineLayout,
                    0, 1, &bindlessDescriptorSet,
                    0, nullptr);

            auto csmShadowMapResult = m_engine->m_caches.imageCache.getImage(m_shadowMapImageId);
            MOE_ASSERT(csmShadowMapResult.has_value(), "Invalid CSM shadow map image id");
            auto csmShadowMap = csmShadowMapResult.value();

            VkUtils::transitionImage(
                    cmdBuffer,
                    csmShadowMap.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            float nearZ = camera.getNearZ();
            float farZ = camera.getFarZ();

            for (int i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
                float cascadeNearZ = i == 0 ? nearZ : m_cascadeFarPlaneZs[i - 1];
                float cascadeFarZ = farZ * m_cascadeSplitRatios[i];
                m_cascadeFarPlaneZs[i] = cascadeFarZ;

                VulkanCamera subFrustumCamera{
                        camera.getPosition(),
                        camera.getPitch(),
                        camera.getYaw(),
                        camera.getFovDeg(),
                        cascadeNearZ,
                        cascadeFarZ,
                };

                float aspect = (float) m_engine->m_drawExtent.width / (float) m_engine->m_drawExtent.height;
                auto corners = subFrustumCamera.getFrustumCornersWorldSpace(aspect);
                m_cascadeLightTransforms[i] = VulkanCamera::getCSMCamera(corners, lightDir, m_csmShadowMapSize, m_shadowMapCameraScale).viewProj;

                auto depthClearValue = VkClearValue{.depthStencil = {1.0f, 0}};
                auto depthAttachment = VkInit::renderingAttachmentInfo(
                        m_shadowMapImageViews[i],
                        &depthClearValue,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                auto renderingInfo = VkInit::renderingInfo(
                        VkExtent2D{m_csmShadowMapSize, m_csmShadowMapSize},
                        nullptr,
                        &depthAttachment);

                vkCmdBeginRendering(cmdBuffer, &renderingInfo);

                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

                auto bindlessSet = m_engine->getBindlessSet().getDescriptorSet();
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &bindlessSet, 0, nullptr);

                const auto viewport = VkViewport{
                        .x = 0,
                        .y = 0,
                        .width = (float) m_csmShadowMapSize,
                        .height = (float) m_csmShadowMapSize,
                        .minDepth = 0.f,
                        .maxDepth = 1.f,
                };
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

                const auto scissor = VkRect2D{
                        .offset = {},
                        .extent = {m_csmShadowMapSize, m_csmShadowMapSize},
                };
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                for (auto& drawCommand: drawCommands) {
                    auto mesh = m_engine->m_caches.meshCache.getMesh(drawCommand.meshId).value();
                    vkCmdBindIndexBuffer(cmdBuffer, mesh.gpuBuffer.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                    auto pushConstants = PushConstants{
                            .mvp = m_cascadeLightTransforms[i] * drawCommand.transform,
                            .vertexBufferAddr =
                                    drawCommand.skinned
                                            ? drawCommand.skinnedVertexBufferAddr
                                            : mesh.gpuBuffer.vertexBufferAddr,
                    };

                    vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
                    vkCmdDrawIndexed(cmdBuffer, mesh.gpuBuffer.indexCount, 1, 0, 0, 0);
                }


                vkCmdEndRendering(cmdBuffer);
            }

            VkUtils::transitionImage(
                    cmdBuffer,
                    csmShadowMap.image,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        void CSMPipeline::destroy() {
            MOE_ASSERT(m_initialized, "CSMPipeline is not initialized");

            for (auto imageView: m_shadowMapImageViews) {
                vkDestroyImageView(m_engine->m_device, imageView, nullptr);
            }

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);

            m_engine = nullptr;
            m_initialized = false;
        }
    }// namespace Pipeline
}// namespace moe