#pragma once

#include "Audio/Common.hpp"

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"
#include "Core/RefCounted.hpp"


MOE_BEGIN_NAMESPACE

constexpr ALuint INVALID_BUFFER_ID = std::numeric_limits<ALuint>::max();

struct AudioBuffer : public AtomicRefCounted<AudioBuffer> {
public:
    ALuint bufferId{INVALID_BUFFER_ID};

    void init();
    bool uploadData(Span<const uint8_t> data, ALenum format, ALsizei freq);
    void destroy();
};

struct AudioBufferPool {
public:
    static constexpr size_t POOL_INIT_SIZE = 32;

    AudioBufferPool() = default;
    ~AudioBufferPool() = default;

    void init();

    void destroy();

    Ref<AudioBuffer> acquireBuffer();

    void handleDeletes() {
        moe::Vector<AudioBuffer*> toDelete;
        {
            std::scoped_lock lk(m_deleteMutex);
            toDelete.swap(m_pendingDeletes);
        }

        for (auto* buffer: toDelete) {
            m_freeBuffers.push_back(buffer);
        }
    }

private:
    Vector<Pinned<AudioBuffer>> m_buffers;
    Deque<AudioBuffer*> m_freeBuffers;
    bool m_initialized{false};

    std::mutex m_deleteMutex;
    Vector<AudioBuffer*> m_pendingDeletes;

    static void bufferDeleter(void* ptr);

    void allocBuffer();
};

MOE_END_NAMESPACE
