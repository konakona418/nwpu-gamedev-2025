#include "Render/Vulkan/Pipeline/PostFXPipeline.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanInitializers.hpp"
#include "Render/Vulkan/VulkanPipeline.hpp"
#include "Render/Vulkan/VulkanUtils.hpp"

namespace moe {
    namespace Pipeline {
        void FXAAPipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "FXAAPipeline already initialized");
            m_initialized = true;
            m_engine = &engine;


            auto vert =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/fxaa.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/fxaa.frag.spv");

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
            // cull nothing. drawing 2d triangles
            builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.disableBlending();
            builder.disableDepthTesting();
            builder.setColorAttachmentFormat(engine.m_drawImageFormat);
            builder.disableMultisampling();

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

        void FXAAPipeline::destroy() {
            MOE_ASSERT(m_initialized, "FXAAPipeline not initialized");

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);

            m_initialized = false;
            m_engine = nullptr;
        }

        void FXAAPipeline::draw(VkCommandBuffer cmdBuffer, ImageId inputImageId) {
            MOE_ASSERT(m_initialized, "FXAAPipeline is not initialized");

            auto extent = m_engine->m_drawExtent;

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

            auto pushConstants = PushConstants{
                    .inputImageId = inputImageId,
                    .screenSize = glm::vec2(extent.width, extent.height),
                    .inverseScreenSize = glm::vec2(1.0f / extent.width, 1.0f / extent.height),
            };

            vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
            vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        }

        void GammaCorrectionPipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "GammaCorrectionPipeline already initialized");
            m_initialized = true;
            m_engine = &engine;

            auto vert =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/gamma_correct.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/gamma_correct.frag.spv");

            auto pushRange = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
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
            builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.disableBlending();
            builder.disableDepthTesting();
            builder.setColorAttachmentFormat(engine.m_swapchainImageFormat);
            builder.disableMultisampling();

            m_pipeline = builder.build(engine.m_device);

            vkDestroyShaderModule(engine.m_device, vert, nullptr);
            vkDestroyShaderModule(engine.m_device, frag, nullptr);
        }

        void GammaCorrectionPipeline::destroy() {
            MOE_ASSERT(m_initialized, "GammaCorrectionPipeline not initialized");

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);

            m_initialized = false;
            m_engine = nullptr;
        }

        void GammaCorrectionPipeline::draw(VkCommandBuffer cmdBuffer, ImageId inputImageId, float gamma) {
            MOE_ASSERT(m_initialized, "GammaCorrectionPipeline is not initialized");

            auto extent = m_engine->m_drawExtent;

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

            auto pushConstants = PushConstants{
                    .inputImageId = inputImageId,
                    .gamma = gamma,
            };

            vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
            vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        }

        void BlendTwoPipeline::init(VulkanEngine& engine) {
            MOE_ASSERT(!m_initialized, "BlendTwoPipeline already initialized");
            m_initialized = true;
            m_engine = &engine;

            auto vert =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/blend_two.vert.spv");
            auto frag =
                    VkUtils::createShaderModuleFromFile(engine.m_device, "shaders/blend_two.frag.spv");

            auto pushRange = VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
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
            builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.disableBlending();
            builder.disableDepthTesting();
            builder.setColorAttachmentFormat(engine.m_drawImageFormat);
            builder.disableMultisampling();

            m_pipeline = builder.build(engine.m_device);

            vkDestroyShaderModule(engine.m_device, vert, nullptr);
            vkDestroyShaderModule(engine.m_device, frag, nullptr);
        }

        void BlendTwoPipeline::destroy() {
            MOE_ASSERT(m_initialized, "BlendTwoPipeline not initialized");

            vkDestroyPipeline(m_engine->m_device, m_pipeline, nullptr);
            vkDestroyPipelineLayout(m_engine->m_device, m_pipelineLayout, nullptr);

            m_initialized = false;
            m_engine = nullptr;
        }

        void BlendTwoPipeline::draw(
                VkCommandBuffer cmdBuffer,
                ImageId inputImageDownId,
                ImageId inputImageUpId,
                float alpha) {
            MOE_ASSERT(m_initialized, "BlendTwoPipeline is not initialized");

            auto extent = m_engine->m_drawExtent;

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

            auto pushConstants = PushConstants{
                    .inputImageUpId = inputImageUpId,
                    .inputImageDownId = inputImageDownId,
                    .alpha = alpha,
            };

            vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
            vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        }
    }// namespace Pipeline
}// namespace moe