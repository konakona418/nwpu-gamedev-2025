#include "Audio/StaticOggProvider.hpp"
#include "Audio/AudioEngine.hpp"

// stb_vorbis is quite simple to integrate,
// and small pieces of audio data is not likely to case performance issues
// so stb_vorbis instead of vorbisfile q(≧▽≦q)
#define STB_VORBIS_IMPLEMENTATION
#include <stb_vorbis.c>

MOE_BEGIN_NAMESPACE

StaticOggProvider::StaticOggProvider(Ref<BinaryBuffer> oggData) : m_oggData(oggData) {
    // decode ogg data
    int channels = 0;
    int sampleRate = 0;
    short* decodedSamples = nullptr;
    int totalSamples = stb_vorbis_decode_memory(
            m_oggData->data(),
            static_cast<int>(m_oggData->size()),
            &channels,
            &sampleRate,
            &decodedSamples);
    if (totalSamples < 0) {
        Logger::error("Failed to decode Ogg Vorbis data");
        return;
    }

    m_sampleRate = static_cast<ALsizei>(sampleRate);
    if (channels == 1) {
        m_format = AL_FORMAT_MONO16;
    } else if (channels == 2) {
        m_format = AL_FORMAT_STEREO16;
    } else {
        Logger::error("Unsupported number of channels in Ogg Vorbis data: {}", channels);

        // stb_vorbis_decode_memory allocates with malloc
        std::free(decodedSamples);
        return;
    }

    size_t totalBytes = static_cast<size_t>(totalSamples) * sizeof(short) * static_cast<size_t>(channels);
    m_decodedData.resize(totalBytes);
    memcpy(m_decodedData.data(), decodedSamples, totalBytes);

    std::free(decodedSamples);
}

StaticOggProvider::~StaticOggProvider() {
    // no-op
}

Ref<AudioBuffer> StaticOggProvider::loadStaticData() {
    auto audioBuffer =
            AudioEngine::getInstance()
                    .getBufferPool()
                    .acquireBuffer();

    if (!audioBuffer->uploadData(m_decodedData, getFormat(), getSampleRate())) {
        Logger::error("Failed to upload static Ogg Vorbis data to AudioBuffer");
        return Ref<AudioBuffer>(nullptr);
    }

    return audioBuffer;
}

MOE_END_NAMESPACE