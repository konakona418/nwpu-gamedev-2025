#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Generator.hpp"
#include "Core/Ref.hpp"
#include "Core/Resource/BinaryBuffer.hpp"
#include "Core/Resource/Image.hpp"


MOE_BEGIN_NAMESPACE

namespace Detail {
    bool loadImageFromMemory(
            const uint8_t* data,
            size_t size,
            Vector<uint8_t>& outImageData,
            int& outWidth,
            int& outHeight,
            int& outChannels,
            int desiredChannels);
}// namespace Detail

template<
        typename InnerGenerator,
        typename = Meta::EnableIfT<
                Meta::IsGeneratorV<InnerGenerator>>,
        typename = Meta::EnableIfT<
                Meta::IsSameV<
                        typename InnerGenerator::value_type,
                        Ref<BinaryBuffer>>>>
struct ImageLoader {
public:
    struct Pref {
        int desiredChannels{4};
    };

    using value_type = Ref<Image>;

    template<typename... Args>
    ImageLoader(Args&&... args)
        : m_derived(std::forward<Args>(args)...) {}

    template<typename... Args>
    ImageLoader(Pref pref, Args&&... args)
        : m_derived(std::forward<Args>(args)...), m_pref(pref) {}

    Optional<value_type> generate() {
        auto bufferOpt = m_derived.generate();
        if (!bufferOpt) {
            return std::nullopt;
        }

        Ref<BinaryBuffer> buffer = *bufferOpt;
        int width, height, channels;
        Vector<uint8_t> imageData;
        if (!Detail::loadImageFromMemory(
                    buffer->data(),
                    buffer->size(),
                    imageData,
                    width,
                    height,
                    channels,
                    m_pref.desiredChannels)) {
            return std::nullopt;
        }

        return Ref(new Image(std::move(imageData), width, height, channels));
    }

private:
    InnerGenerator m_derived;
    Pref m_pref;
};

MOE_END_NAMESPACE