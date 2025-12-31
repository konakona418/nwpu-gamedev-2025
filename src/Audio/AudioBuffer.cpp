#include "Audio/AudioBuffer.hpp"
#include "Audio/AudioEngine.hpp"

MOE_BEGIN_NAMESPACE

void AudioBuffer::init() {
    MOE_ASSERT(bufferId == INVALID_BUFFER_ID, "AudioBuffer already initialized");
    alGenBuffers(1, &bufferId);
}

bool AudioBuffer::uploadData(Span<const uint8_t> data, ALenum format, ALsizei freq) {
    MOE_ASSERT(bufferId != INVALID_BUFFER_ID, "AudioBuffer not initialized");

    alGetError();
    alBufferData(
            bufferId,
            format,
            data.data(),
            static_cast<ALsizei>(data.size()),
            freq);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        Logger::error("Failed to upload audio buffer data: AL error code {}", error);
        return false;
    }
    return true;
}

void AudioBuffer::destroy() {
    MOE_ASSERT(bufferId != INVALID_BUFFER_ID, "AudioBuffer not initialized");
    alDeleteBuffers(1, &bufferId);
    bufferId = INVALID_BUFFER_ID;
}

void AudioBufferPool::init() {
    for (size_t i = 0; i < POOL_INIT_SIZE; i++) {
        allocBuffer();
    }
    m_initialized = true;
}

void AudioBufferPool::destroy() {
    for (auto& buffer: m_buffers) {
        buffer->destroy();
    }
    m_buffers.clear();
    m_freeBuffers.clear();

    m_initialized = false;
}

Ref<AudioBuffer> AudioBufferPool::acquireBuffer() {
    MOE_ASSERT(m_initialized, "AudioBufferPool not initialized");

    if (m_freeBuffers.empty()) {
        // no pending buffers, allocate a new one
        allocBuffer();
    }

    AudioBuffer* buffer = m_freeBuffers.back();
    m_freeBuffers.pop_back();

    return Ref<AudioBuffer>(buffer);
}

void AudioBufferPool::bufferDeleter(void* ptr) {
    // Rc reached zero, return to pool
    if (std::this_thread::get_id() == AudioEngine::getInstance().getAudioThreadId()) {
        // on audio thread, can push directly
        moe::Logger::debug("AudioBufferPool::bufferDeleter: called from audio thread, "
                           "recycling buffer {}",
                           static_cast<AudioBuffer*>(ptr)->bufferId);
        AudioEngine::getInstance()
                .getBufferPool()
                .m_freeBuffers
                .push_back(static_cast<AudioBuffer*>(ptr));
        return;
    }

    moe::Logger::debug("AudioBufferPool::bufferDeleter: called from non-audio thread, "
                       "deferring buffer recycling of buffer {}",
                       static_cast<AudioBuffer*>(ptr)->bufferId);

    auto& bufferPool = AudioEngine::getInstance().getBufferPool();
    std::lock_guard<std::mutex> lock(bufferPool.m_deleteMutex);
    bufferPool.m_pendingDeletes.push_back(static_cast<AudioBuffer*>(ptr));
}

void AudioBufferPool::allocBuffer() {
    if (m_buffers.size() >= 128) {
        Logger::warn("AudioBufferPool allocated too many buffers({})", m_buffers.size());
    }

    auto buffer = makePinned<AudioBuffer>();
    buffer->init();
    buffer->setDeleter(AudioBufferPool::bufferDeleter);

    m_freeBuffers.push_back(buffer.get());
    m_buffers.push_back(std::move(buffer));
}

MOE_END_NAMESPACE
