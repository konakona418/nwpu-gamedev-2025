#pragma once

#include "Audio/AudioDataProvider.hpp"
#include "Audio/Common.hpp"

#include "Core/Resource/BinaryBuffer.hpp"


MOE_BEGIN_NAMESPACE

struct StaticOggProvider : public AudioDataProvider {
public:
    StaticOggProvider(Ref<BinaryBuffer> oggData);
    ~StaticOggProvider() override;

    ALuint getFormat() const override { return m_format; }
    ALsizei getSampleRate() const override { return m_sampleRate; }

    bool isStreaming() const override { return false; }

    Ref<AudioBuffer> loadStaticData() override;

    Ref<AudioBuffer> streamNextPacket(size_t* outSize) override {
        MOE_ASSERT(false, "StaticOggProvider does not support streaming");
        return Ref<AudioBuffer>(nullptr);
    }

    void seekToStart() override {
        MOE_ASSERT(false, "StaticOggProvider does not support seeking");
        // no-op
    }

private:
    Ref<BinaryBuffer> m_oggData;
    Vector<uint8_t> m_decodedData;
    ALuint m_format{0};
    ALsizei m_sampleRate{0};
};

MOE_END_NAMESPACE