#pragma once

#include "Render/Vulkan/VulkanIdTypes.hpp"
#include "Render/Vulkan/VulkanTypes.hpp"

namespace moe {
    class VulkanEngine;
}


namespace moe {
    namespace Pipeline {
        struct FXAAPipeline {
        public:
            FXAAPipeline() = default;
            ~FXAAPipeline() = default;

            void init(VulkanEngine& engine);

            void draw(
                    VkCommandBuffer cmdBuffer,
                    ImageId inputImageId);

            void destroy();

        private:
            struct PushConstants {
                ImageId inputImageId;
                glm::vec2 screenSize;
                glm::vec2 inverseScreenSize;
            };

            VulkanEngine* m_engine{nullptr};
            bool m_initialized{false};

            VkPipelineLayout m_pipelineLayout;
            VkPipeline m_pipeline;
        };

        struct GammaCorrectionPipeline {
        public:
            static constexpr float DEFAULT_GAMMA = 2.2f;

            GammaCorrectionPipeline() = default;
            ~GammaCorrectionPipeline() = default;

            void init(VulkanEngine& engine);

            void draw(
                    VkCommandBuffer cmdBuffer,
                    ImageId inputImageId,
                    float gamma = DEFAULT_GAMMA);

            void destroy();

        private:
            struct PushConstants {
                ImageId inputImageId;
                float gamma;
            };

            VulkanEngine* m_engine{nullptr};
            bool m_initialized{false};

            VkPipelineLayout m_pipelineLayout;
            VkPipeline m_pipeline;
        };

        struct BlendTwoPipeline {
        public:
            BlendTwoPipeline() = default;
            ~BlendTwoPipeline() = default;

            void init(VulkanEngine& engine);

            void draw(
                    VkCommandBuffer cmdBuffer,
                    ImageId inputImageDownId,
                    ImageId inputImageUpId,
                    float alpha);

            void destroy();

        private:
            struct PushConstants {
                ImageId inputImageUpId;
                ImageId inputImageDownId;
                float alpha;
            };

            VulkanEngine* m_engine{nullptr};
            bool m_initialized{false};

            VkPipelineLayout m_pipelineLayout;
            VkPipeline m_pipeline;
        };

    }// namespace Pipeline
}// namespace moe