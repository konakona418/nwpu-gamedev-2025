#include "Audio/AudioEngine.hpp"
#include "Audio/AudioBuffer.hpp"
#include "Audio/AudioSource.hpp"

MOE_BEGIN_NAMESPACE

static const char* OPENAL_SOFT_DEVICE_NAME = "OpenAL Soft";

std::thread::id AudioEngine::s_audioThreadId;

void AudioEngine::handleCommands() {
    std::scoped_lock lk(m_mutex);
    while (!m_commandQueue.empty()) {
        auto& cmd = m_commandQueue.front();
        cmd->execute(*this);
        if (cmd->isBlocking()) {
            auto blockingCmd = static_cast<BlockingAudioCommand*>(cmd.get());
            blockingCmd->notify();
        }
        m_commandQueue.pop_front();
    }
}

void AudioEngine::mainAudioLoop() {
    auto lastTime = std::chrono::steady_clock::now();
    constexpr auto frameDuration = std::chrono::milliseconds(5);
    while (m_running.load()) {
        // process audio commands
        handleCommands();
        m_bufferPool.handleDeletes();

        // update audio sources
        for (auto& source: m_sources) {
            source->update();
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = currentTime - lastTime;
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        } else {
            Logger::warn("Audio engine main loop is running behind");
        }
        lastTime = std::chrono::steady_clock::now();
    }

    handleCommands();// final command handling before exit
    m_bufferPool.handleDeletes();
}

void AudioEngine::initAndLaunchMainAudioLoop() {
    m_running.store(true);
    m_audioThread = std::thread([this]() {
        Logger::setThreadName("Audio");
        s_audioThreadId = std::this_thread::get_id();
        Logger::info("Audio engine main loop started");

        static std::once_flag initFlag;
        std::call_once(initFlag, [this]() {
            MOE_ASSERT(!m_initialized, "AudioEngine already initialized");

            std::scoped_lock lk(m_mutex);// lock during init

            auto device = alcOpenDevice(OPENAL_SOFT_DEVICE_NAME);// OpenAL Soft preferred
            if (device) {
                const ALCchar* deviceName = alcGetString(device, ALC_DEVICE_SPECIFIER);

                Logger::info("Opened OpenAL device: {}", deviceName);

                auto context = alcCreateContext(device, nullptr);
                alcMakeContextCurrent(context);

                bool eaxSupported = alcIsExtensionPresent(device, "EAX2.0");
                m_eaxSupported = eaxSupported;
                if (eaxSupported) {
                    Logger::info("EAX 2.0 is supported");
                } else {
                    Logger::info("EAX 2.0 is not supported");
                }
            } else {
                // ensure OpenAL Soft is used
                // if OpenAL dynamic library is missing, system default OpenAL implementation may be used instead
                // and that may cause various issues
                Logger::critical("Preferred OpenAL device '{}' not found. "
                                 "Check if dynamic linked library 'OpenAL32.dll'/'libopenal.so' is present",
                                 OPENAL_SOFT_DEVICE_NAME);
                m_initialized = false;

                return;
            }

            if (alGetError() != AL_NO_ERROR) {
                Logger::critical("OpenAL error occurred, failing to initialize audio engine");
                m_initialized = false;
                return;
            }

            Logger::info("Audio engine initialized successfully.");

            Logger::info("Initializing default audio listener...");
            m_listener.init();

            m_initialized = true;

            m_bufferPool.init();
        });

        mainAudioLoop();
    });
}

void AudioEngine::init() {
    initAndLaunchMainAudioLoop();
}

void AudioEngine::cleanup() {
    MOE_ASSERT(m_initialized, "AudioEngine not initialized");

    m_running.store(false);
    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }

    Logger::info("Cleaning up audio engine...");

    m_bufferPool.destroy();

    m_listener.destroy();

    auto context = alcGetCurrentContext();
    auto device = alcGetContextsDevice(context);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);

    Logger::info("Audio engine cleaned up.");
}

AudioEngineInterface AudioEngine::getInterface() {
    return AudioEngineInterface(AudioEngine::getInstance());
}

Ref<AudioSource> AudioEngineInterface::createAudioSource() {
    Ref<AudioSource> source;

    std::promise<void> promise;
    auto future = promise.get_future();

    submitCommand(
            std::make_unique<CreateSourceCommand>(
                    &source, std::move(promise)));
    future.get();

    Logger::debug("Audio source created with id {}", source->sourceId());

    return source;
}

Vector<Ref<AudioSource>> AudioEngineInterface::createAudioSources(size_t count) {
    Vector<Ref<AudioSource>> sources;

    std::promise<void> promise;
    auto future = promise.get_future();

    submitCommand(
            std::make_unique<CreateSourcesCommand>(
                    &sources, count, std::move(promise)));
    future.get();

    Logger::debug("{} audio sources created", count);

    return sources;
}

void AudioEngineInterface::loadAudioSource(
        Ref<AudioSource> source,
        Ref<AudioDataProvider> provider, bool loop) {
    submitCommand(
            std::make_unique<SourceLoadCommand>(
                    std::move(source),
                    std::move(provider),
                    loop));
}

void AudioEngineInterface::playAudioSource(Ref<AudioSource> source) {
    submitCommand(
            std::make_unique<SourcePlayCommand>(
                    std::move(source)));
}

void AudioEngineInterface::pauseAudioSource(Ref<AudioSource> source) {
    submitCommand(
            std::make_unique<SourcePauseCommand>(
                    std::move(source)));
}

void AudioEngineInterface::stopAudioSource(Ref<AudioSource> source) {
    submitCommand(
            std::make_unique<SourceStopCommand>(
                    std::move(source)));
}

void AudioEngineInterface::setAudioSourcePosition(Ref<AudioSource> source, float x, float y, float z) {
    submitCommand(
            std::make_unique<SourcePositionUpdateCommand>(
                    std::move(source), x, y, z));
}

void AudioEngineInterface::setListenerPosition(const glm::vec3& position) {
    submitCommand(
            std::make_unique<SetListenerPositionCommand>(
                    position));
}

void AudioEngineInterface::setListenerVelocity(const glm::vec3& velocity) {
    submitCommand(
            std::make_unique<SetListenerVelocityCommand>(
                    velocity));
}

void AudioEngineInterface::setListenerOrientation(const glm::vec3& forward, const glm::vec3& up) {
    submitCommand(
            std::make_unique<SetListenerOrientationCommand>(
                    forward, up));
}

void AudioEngineInterface::setListenerGain(float gain) {
    submitCommand(
            std::make_unique<SetListenerGainCommand>(
                    gain));
}

MOE_END_NAMESPACE