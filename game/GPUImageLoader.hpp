#pragma once

#include "Core/Meta/Generator.hpp"
#include "Core/Meta/TypeTraits.hpp"
#include "Core/Resource/BinaryBuffer.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanIdTypes.hpp"


namespace game {
    template<typename InnerGenerator,
             typename = moe::Meta::EnableIfT<moe::Meta::IsGeneratorV<InnerGenerator>>,
             typename = moe::Meta::EnableIfT<moe::Meta::IsSameV<typename InnerGenerator::value_type, moe::Ref<moe::Image>>>>
    struct GPUImageLoader {
    public:
        using value_type = moe::ImageId;

        template<typename... Args>
        GPUImageLoader(Args&&... args)
            : m_derived(std::forward<Args>(args)...) {}

        moe::Optional<value_type> generate() {
            moe::Optional<moe::Ref<moe::Image>> imageOpt = m_derived.generate();
            if (!imageOpt.has_value()) {
                return std::nullopt;
            }

            auto image = *imageOpt;
            if (!image) {
                return std::nullopt;
            }

            auto imageId = moe::VulkanEngine::get()
                                   .getResourceLoader()
                                   .load(moe::Loader::Image, *image.get());

            if (imageId == moe::NULL_IMAGE_ID) {
                return std::nullopt;
            }

            return imageId;
        }

        uint64_t hashCode() const {
            return m_derived.hashCode();
        }

        moe::String paramString() const {
            return fmt::format("gpu_image_loader_{}", m_derived.paramString());
        }

    private:
        InnerGenerator m_derived;
    };
}// namespace game