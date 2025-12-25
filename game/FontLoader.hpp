#pragma once

#include "Core/Meta/Generator.hpp"
#include "Core/Meta/TypeTraits.hpp"
#include "Core/Resource/BinaryBuffer.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanIdTypes.hpp"


namespace game {
    struct FontLoaderParam {
        float fontSize{16.0f};
        moe::StringView glyphRange{""};

        FontLoaderParam() = default;
        explicit FontLoaderParam(float size)
            : fontSize(size) {}

        FontLoaderParam(float size, moe::StringView range)
            : fontSize(size), glyphRange(range) {}

        FontLoaderParam& setFontSize(float size) {
            fontSize = size;
            return *this;
        }

        FontLoaderParam& setGlyphRange(moe::StringView range) {
            glyphRange = range;
            return *this;
        }
    };

    template<typename InnerGenerator,
             typename = moe::Meta::EnableIfT<moe::Meta::IsGeneratorV<InnerGenerator>>,
             typename = moe::Meta::EnableIfT<moe::Meta::IsSameV<typename InnerGenerator::value_type, moe::Ref<moe::BinaryBuffer>>>>
    struct FontLoader {
    public:
        using value_type = moe::FontId;

        template<typename... Args>
        FontLoader(FontLoaderParam param, Args&&... args)
            : m_fontSize(param.fontSize), m_glyphRange(param.glyphRange), m_derived(std::forward<Args>(args)...) {}

        template<typename... Args>
        FontLoader(Args&&... args) : m_derived(std::forward<Args>(args)...) {}

        moe::Optional<value_type> generate() {
            moe::Optional<moe::Ref<moe::BinaryBuffer>> binaryBuffer = m_derived.generate();
            if (!binaryBuffer.has_value()) {
                return std::nullopt;
            }

            auto buffer = *binaryBuffer;
            if (!buffer || buffer->size() == 0) {
                return std::nullopt;
            }

            auto fontId = moe::VulkanEngine::get().getResourceLoader().load(
                    moe::Loader::Font,
                    moe::Span<const uint8_t>(
                            buffer->data(),
                            buffer->size()),
                    m_fontSize,
                    m_glyphRange);

            return fontId;
        }

        // ! assume the inner generator's hashCode is good enough
        moe::String paramString() const {
            return fmt::format("font_loader_{}", m_derived.paramString());
        }

        uint64_t hashCode() const {
            return m_derived.hashCode();
        }

    private:
        InnerGenerator m_derived;
        float m_fontSize{16.0f};
        moe::StringView m_glyphRange{""};
    };
}// namespace game