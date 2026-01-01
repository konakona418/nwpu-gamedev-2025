#pragma once

#include "Audio/AudioBuffer.hpp"
#include "Audio/AudioCommand.hpp"
#include "Audio/AudioListener.hpp"
#include "Audio/AudioSource.hpp"
#include "Audio/Common.hpp"


#include "Core/Meta/Feature.hpp"
#include "Core/Ref.hpp"


MOE_BEGIN_NAMESPACE

struct AudioSource;
struct AudioEngineInterface;
struct AudioDataProvider;

class AudioEngine : public Meta::Singleton<AudioEngine> {
public:
    friend struct AudioEngineInterface;

    MOE_SINGLETON(AudioEngine)

    AudioListener& getListener() {
        return m_listener;
    }

    static AudioEngineInterface getInterface();

    Vector<Pinned<AudioSource>>& getSources() {
        return m_sources;
    }

    AudioBufferPool& getBufferPool() { return m_bufferPool; }

    std::thread::id getAudioThreadId() const {
        return s_audioThreadId;
    }

private:
    static std::thread::id s_audioThreadId;

    bool m_initialized{false};
    bool m_eaxSupported{false};

    AudioBufferPool m_bufferPool;

    Vector<Pinned<AudioSource>> m_sources;

    Deque<UniquePtr<AudioCommand>> m_commandQueue;
    std::mutex m_mutex;

    AudioListener m_listener;

    std::thread m_audioThread;
    std::atomic_bool m_running{false};

    AudioEngine() {
        init();
    };

    ~AudioEngine() {
        cleanup();
    }

    void mainAudioLoop();
    void initAndLaunchMainAudioLoop();
    void handleCommands();

    void init();
    void cleanup();
};

struct AudioEngineInterface {
public:
    friend class AudioEngine;

    ~AudioEngineInterface() = default;

    void submitCommand(UniquePtr<AudioCommand> command) {
        if (std::this_thread::get_id() == AudioEngine::s_audioThreadId) {
            // direct execute
            // to prevent lock recursion
            command->execute(*m_engine);
            return;
        }
        std::lock_guard<std::mutex> lock(m_engine->m_mutex);
        m_engine->m_commandQueue.push_back(std::move(command));
    }

    Ref<AudioSource> createAudioSource();
    Vector<Ref<AudioSource>> createAudioSources(size_t count);
    void loadAudioSource(Ref<AudioSource> source, Ref<AudioDataProvider> provider, bool loop);
    void playAudioSource(Ref<AudioSource> source);
    void pauseAudioSource(Ref<AudioSource> source);
    void stopAudioSource(Ref<AudioSource> source);
    void setAudioSourcePosition(Ref<AudioSource> source, float x, float y, float z);

    void setListenerPosition(const glm::vec3& position);
    void setListenerVelocity(const glm::vec3& velocity);
    void setListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
    void setListenerGain(float gain);

private:
    AudioEngine* m_engine;

    AudioEngineInterface(AudioEngine& engine)
        : m_engine(&engine) {}
};

MOE_END_NAMESPACE