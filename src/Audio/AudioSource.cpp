#include "Audio/AudioSource.hpp"
#include "Audio/AudioEngine.hpp"
#include <algorithm>

MOE_BEGIN_NAMESPACE

AudioSource::AudioSource() {
    alGenSources(1, &m_sourceId);
}

void AudioSource::load(Ref<AudioDataProvider> provider, bool loop) {
    m_provider = provider;
    m_loop = loop;

    if (!m_provider->isStreaming()) {
        loadStaticBuffer();
    } else {
        loadStreamInitialBuffers();
    }
}

void AudioSource::play() {
    m_isPlaying = true;
    alSourcePlay(m_sourceId);
}

void AudioSource::pause() {
    m_isPlaying = false;
    alSourcePause(m_sourceId);
}

void AudioSource::stop() {
    m_isPlaying = false;
    alSourceStop(m_sourceId);
}

void AudioSource::disableAttenuation() {
    alSourcef(m_sourceId, AL_ROLLOFF_FACTOR, 0.0f);
}

void AudioSource::update() {
    if (!m_provider) {
        // n/a
        return;
    }

    if (!m_provider->isStreaming()) {
        return;
    }

    ALint processedBuffers = 0;
    alGetSourcei(m_sourceId, AL_BUFFERS_PROCESSED, &processedBuffers);

    while (processedBuffers-- > 0) {
        auto buffer = m_queuedBuffers.front();
        alSourceUnqueueBuffers(m_sourceId, 1, &buffer->bufferId);
        m_queuedBuffers.pop();

        size_t packetSize = 0;
        Ref<AudioBuffer> newBuffer = m_provider->streamNextPacket(&packetSize);
        if (!newBuffer) {
            // eof reached
            if (m_loop) {
                m_provider->seekToStart();
                newBuffer = m_provider->streamNextPacket(&packetSize);
                if (!newBuffer) {
                    Logger::error("Failed to loop audio stream, no data after seek");
                    continue;
                }
            } else {
                continue;
            }
        }

        alSourceQueueBuffers(m_sourceId, 1, &newBuffer->bufferId);
        m_queuedBuffers.push(newBuffer);
    }

    ALint currentQueued = 0;
    alGetSourcei(m_sourceId, AL_BUFFERS_QUEUED, &currentQueued);

    ALint sourceState = 0;
    alGetSourcei(m_sourceId, AL_SOURCE_STATE, &sourceState);

    if (sourceState != AL_PLAYING && currentQueued > 0 && m_isPlaying) {
        Logger::debug("Restarting audio source {}", m_sourceId);
        alSourcePlay(m_sourceId);
    }
}

void AudioSource::loadStreamInitialBuffers() {
    for (size_t i = 0; i < MAX_STREAMING_BUFFERS; i++) {
        size_t packetSize = 0;
        Ref<AudioBuffer> buffer = m_provider->streamNextPacket(&packetSize);
        if (!buffer) {
            // eof reached
            break;
        }

        alSourceQueueBuffers(m_sourceId, 1, &buffer->bufferId);
        m_queuedBuffers.push(buffer);
    }
}

void AudioSource::loadStaticBuffer() {
    if (!m_provider) {
        // n/a
        return;
    }

    Ref<AudioBuffer> buffer = m_provider->loadStaticData();
    if (!buffer) {
        Logger::error("Failed to load static audio data");
        return;
    }

    m_queuedBuffers.push(buffer);
    alSourcei(m_sourceId, AL_BUFFER, static_cast<ALint>(buffer->bufferId));
}

static void sourceDeleter(void* ptr) {
    AudioEngine::getInterface()
            .submitCommand(
                    std::make_unique<DestroySourceCommand>(
                            static_cast<AudioSource*>(ptr)));
}

void CreateSourceCommand::execute(AudioEngine& engine) {
    Pinned<AudioSource> sourcePinned = makePinned<AudioSource>();

    auto source = Ref<AudioSource>(sourcePinned.get());
    source->setDeleter(sourceDeleter);

    engine.getSources().push_back(std::move(sourcePinned));

    *outSource = source;
}

void CreateSourcesCommand::execute(AudioEngine& engine) {
    Vector<Ref<AudioSource>> sources;
    sources.reserve(count);

    Vector<ALuint> rawSources(count);
    alGenSources(static_cast<ALsizei>(count), rawSources.data());

    for (size_t i = 0; i < count; ++i) {
        Pinned<AudioSource> sourcePinned = makePinned<AudioSource>(rawSources[i]);

        auto source = Ref<AudioSource>(sourcePinned.get());
        source->setDeleter(sourceDeleter);

        engine.getSources().push_back(std::move(sourcePinned));
        sources.push_back(source);
    }

    *outSources = std::move(sources);
}

void DestroySourceCommand::execute(AudioEngine& engine) {
    auto id = source->sourceId();

    source->manualDestroy();

    auto& sources = engine.getSources();
    sources.erase(
            std::remove_if(
                    sources.begin(),
                    sources.end(),
                    [this](const Pinned<AudioSource>& s) {
                        return s.get() == source;
                    }),
            sources.end());

    Logger::debug("Audio source id {} removed", id);
}

void SourceLoadCommand::execute(AudioEngine& engine) {
    Logger::debug("Loading audio data for source {}", source->sourceId());
    source->load(provider, loop);
}

void SourcePlayCommand::execute(AudioEngine& engine) {
    Logger::debug("Play audio source {}", source->sourceId());
    source->play();
}

void SourcePauseCommand::execute(AudioEngine& engine) {
    Logger::debug("Pause audio source {}", source->sourceId());
    source->pause();
}

void SourceStopCommand::execute(AudioEngine& engine) {
    Logger::debug("Stop audio source {}", source->sourceId());
    source->stop();
}

void SourceDisableAttenuationCommand::execute(AudioEngine& engine) {
    Logger::debug("Disable attenuation for audio source {}", source->sourceId());
    source->disableAttenuation();
}

void SourcePositionUpdateCommand::execute(AudioEngine& engine) {
    Logger::debug("Update position for audio source {}", source->sourceId());
    source->setPosition(x, y, z);
}

MOE_END_NAMESPACE