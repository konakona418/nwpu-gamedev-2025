#pragma once

#include "Audio/AudioBuffer.hpp"
#include "Audio/AudioCommand.hpp"
#include "Audio/AudioDataProvider.hpp"
#include "Audio/Common.hpp"

#include "Core/RefCounted.hpp"

MOE_BEGIN_NAMESPACE

struct AudioSource : public AtomicRefCounted<AudioSource> {
public:
    static constexpr size_t MAX_STREAMING_BUFFERS = 8;

    AudioSource();
    ~AudioSource() = default;

    void manualDestroy() {
        alDeleteSources(1, &m_sourceId);
    }

    void load(Ref<AudioDataProvider> provider, bool loop = false);

    void play();
    void pause();
    void stop();

    void disableAttenuation();

    void update();

    ALuint sourceId() const {
        return m_sourceId;
    }

private:
    ALuint m_sourceId{0};

    Ref<AudioDataProvider> m_provider;
    bool m_loop{false};
    bool m_isPlaying{false};

    Queue<Ref<AudioBuffer>> m_queuedBuffers;

    void loadStreamInitialBuffers();
    void loadStaticBuffer();
};

struct CreateSourceCommand : public BlockingAudioCommand {
    Ref<AudioSource>* outSource;

    explicit CreateSourceCommand(Ref<AudioSource>* outSrc, std::promise<void>&& promise)
        : BlockingAudioCommand(std::move(promise)), outSource(outSrc) {}

    void execute(AudioEngine& engine) override;
};

struct DestroySourceCommand : public AudioCommand {
    AudioSource* source;

    explicit DestroySourceCommand(AudioSource* src)
        : source(std::move(src)) {}

    void execute(AudioEngine& engine) override;
};

struct SourceLoadCommand : public AudioCommand {
    Ref<AudioSource> source;
    Ref<AudioDataProvider> provider;
    bool loop;

    SourceLoadCommand(Ref<AudioSource> src, Ref<AudioDataProvider> prov, bool lp)
        : source(std::move(src)), provider(std::move(prov)), loop(lp) {}

    void execute(AudioEngine& engine) override;
};

struct SourcePlayCommand : public AudioCommand {
    Ref<AudioSource> source;

    explicit SourcePlayCommand(Ref<AudioSource> src)
        : source(std::move(src)) {}

    void execute(AudioEngine& engine) override;
};

struct SourcePauseCommand : public AudioCommand {
    Ref<AudioSource> source;

    explicit SourcePauseCommand(Ref<AudioSource> src)
        : source(std::move(src)) {}

    void execute(AudioEngine& engine) override;
};

struct SourceStopCommand : public AudioCommand {
    Ref<AudioSource> source;

    explicit SourceStopCommand(Ref<AudioSource> src)
        : source(std::move(src)) {}

    void execute(AudioEngine& engine) override;
};

struct SourceDisableAttenuationCommand : public AudioCommand {
    Ref<AudioSource> source;

    explicit SourceDisableAttenuationCommand(Ref<AudioSource> src)
        : source(std::move(src)) {}

    void execute(AudioEngine& engine) override;
};

MOE_END_NAMESPACE