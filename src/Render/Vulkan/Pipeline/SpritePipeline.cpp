#include "Render/Vulkan/Pipeline/SpritePipeline.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanInitializers.hpp"
#include "Render/Vulkan/VulkanPipeline.hpp"
#include "Render/Vulkan/VulkanUtils.hpp"


namespace moe {
    namespace Pipeline {
        void SpritePipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "SpritePipeline already initialized");
            m_engine = &engine;
            auto vert =
                    VkUtils::createShaderModuleFromFile(
                            engine.m_device,
                            "shaders/sprite.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(
                            engine.m_device,
                            "shaders/sprite.frag.spv");

            auto pushRangeTransform = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = sizeof(PushConstants),
            };

            auto pushRanges =
                    Array<VkPushConstantRange, 1>{pushRangeTransform};

            VulkanDescriptorLayoutBuilder layoutBuilder{};

            auto descriptorLayouts = Array<VkDescriptorSetLayout, 1>{
                    engine.getBindlessSet().getDescriptorSetLayout(),
            };

            auto pipelineLayoutInfo =
                    VkInit::pipelineLayoutCreateInfo(descriptorLayouts, pushRanges);

            MOE_VK_CHECK(vkCreatePipelineLayout(engine.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

            Logger::info("Sprite rendering antialiasing is currently disabled");
            auto builder = VulkanPipelineBuilder(m_pipelineLayout);
            builder.addShader(vert, frag);
            builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.enableBlending();
            builder.setColorAttachmentFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
            builder.setDepthFormat(VK_FORMAT_D32_SFLOAT);
            builder.disableMultisampling();
            builder.disableDepthTesting();

            m_pipeline = builder.build(engine.m_device);

            vkDestroyShaderModule(engine.m_device, vert, nullptr);
            vkDestroyShaderModule(engine.m_device, frag, nullptr);

            m_initialized = true;
        }

        void SpritePipeline::draw(
                VkCommandBuffer cmdBuffer,
                VulkanMeshCache& meshCache,
                Span<VulkanSprite> sprites,
                const glm::mat4& viewProj,
                VulkanAllocatedImage& renderTarget) {
            MOE_ASSERT(m_initialized, "SpritePipeline not initialized");

            VkClearValue clearColor{.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};

            auto colorAttachment = VkInit::renderingAttachmentInfo(
                    renderTarget.imageView,
                    &clearColor,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            auto extent = VkExtent2D{
                    .width = m_engine->m_drawExtent.width,
                    .height = m_engine->m_drawExtent.height,
            };
            auto renderInfo = VkInit::renderingInfo(extent, &colorAttachment, nullptr);

            vkCmdBeginRendering(cmdBuffer, &renderInfo);
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            auto bindlessDescriptorSet = m_engine->getBindlessSet().getDescriptorSet();
            vkCmdBindDescriptorSets(
                    cmdBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipelineLayout,
                    0, 1, &bindlessDescriptorSet,
                    0, nullptr);

            const auto viewport = VkViewport{
                    .x = 0,
                    .y = 0,
                    .width = (float) extent.width,
                    .height = (float) extent.height,
                    .minDepth = 0.f,
                    .maxDepth = 1.f,
            };
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            const auto scissor = VkRect2D{
                    .offset = {},
                    .extent = extent,
            };
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            for (auto& sprite: sprites) {
                auto mesh = *meshCache.getMesh(meshCache.defaults.rectMeshId);

                glm::vec2 texSize{1, 1};
                if (sprite.textureId != NULL_IMAGE_ID) {
                    auto tex = m_engine->m_caches.imageCache.getImage(sprite.textureId);
                    MOE_ASSERT(tex.has_value(), "Invalid texture id");

                    texSize.x = tex->imageExtent.width;
                    texSize.y = tex->imageExtent.height;
                }

                PushConstants pushConstantsTransform{
                        .transform = viewProj * sprite.transform.getMatrix(),

                        .color = sprite.color.toVec4(),
                        .spriteSize = sprite.size,
                        .texRegionOffset = sprite.texOffset,
                        .texRegionSize = sprite.texSize,
                        .textureSize = texSize,
                        .textureId = sprite.textureId,
                        .isTextSprite = sprite.isTextSprite,
                };

                vkCmdPushConstants(
                        cmdBuffer,
                        m_pipelineLayout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(PushConstants),
                        &pushConstantsTransform);

                vkCmdBindIndexBuffer(cmdBuffer, mesh.gpuBuffer.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmdBuffer, mesh.gpuBuffer.indexCount, 1, 0, 0, 0);
            }

            vkCmdEndRendering(cmdBuffer);
        }

        void SpritePipeline::destroy() {
            MOE_ASSERT(m_initialized, "SpritePipeline not initialized");

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);
        }
    }// namespace Pipeline
}// namespace moe