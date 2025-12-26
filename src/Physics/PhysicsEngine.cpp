#include "Physics/PhysicsEngine.hpp"

MOE_BEGIN_NAMESPACE

void PhysicsEngine::init() {
    Logger::info("Initializing physics engine...");

    JPH::RegisterDefaultAllocator();

    JPH::Trace = Physics::Details::TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = Physics::Details::AssertFailedImpl;)

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    Logger::info("Jolt Physics version: {}.{}.{}",
                 JPH_VERSION_MAJOR,
                 JPH_VERSION_MINOR,
                 JPH_VERSION_PATCH);
    Logger::info("Initializing physics utilities...");
    m_broadPhaseLayerInterface = std::make_unique<Physics::Details::BPLayerInterfaceImpl>();
    m_objectVsBroadPhaseLayerFilter = std::make_unique<Physics::Details::ObjectVsBroadPhaseLayerFilterImpl>();
    m_objectLayerFilter = std::make_unique<Physics::Details::ObjectLayerFilterImpl>();
    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(1024 * 1024);
    m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers);

    Logger::info("Initializing physics system...");
    m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_physicsSystem->Init(
            MAX_BODIES, NUM_BODY_MUTEXES,
            MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
            *m_broadPhaseLayerInterface,
            *m_objectVsBroadPhaseLayerFilter,
            *m_objectLayerFilter);

    Logger::info("Physics system initialized with {} max bodies, {} max body pairs, {} max contact constraints",
                 MAX_BODIES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS);
    Logger::info("Launching physics thread...");
    m_initialized = true;
    launchPhysicsThread();

    Logger::info("Physics engine initialized");
}

void PhysicsEngine::destroy() {
    MOE_ASSERT(m_initialized, "Physics engine not initialized");
    Logger::info("Shutting down physics engine...");
    m_running.store(false);
    if (m_physicsThread.joinable()) {
        m_physicsThread.join();
    }

    m_persistOnPhysicsThreadFn.clear();
    m_dispatchedOnPhysicsThreadFn.clear();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    Logger::info("Physics engine shut down");
    m_initialized = false;
}

void PhysicsEngine::launchPhysicsThread() {
    m_running.store(true);
    m_physicsThread = std::thread(&PhysicsEngine::mainLoop, this);
}

void PhysicsEngine::mainLoop() {
    Logger::setThreadName("Physics");
    Logger::info("Physics thread started");

    const auto step = std::chrono::duration_cast<std::chrono::nanoseconds>(PHYSICS_TIMESTEP);
    auto nextTick = std::chrono::high_resolution_clock::now();

    auto _lastTime = std::chrono::high_resolution_clock::now();

    while (m_running.load()) {
        m_physicsSystem->Update(PHYSICS_TIMESTEP.count(), 1, m_tempAllocator.get(), m_jobSystem.get());

        syncPhysicsToSwapBuffer();
        executeDispatchedFunctions();

        nextTick += step;

        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto remaining = nextTick - now;

            // sleeping is not precise enough for small durations
            // i have no idea why
            // so just busy wait
            // ! fixme: find out the reason and fix it
            if (remaining > std::chrono::nanoseconds(0)) {
                // yield to other threads
                // still busy waiting though φ(゜▽゜*)♪
                std::this_thread::yield();
            } else {
                break;
            }

            // extremely large lag compensation
            if (remaining < -step * 5) {
                nextTick = now;
                break;
            }
        }

        // advance tick index
        m_currentTickIndex.fetch_add(1);

        {
            // stats
            auto _now = std::chrono::high_resolution_clock::now();
            auto _dt = _now - _lastTime;
            _lastTime = _now;

            m_stats.physicsFrameTime = std::chrono::duration_cast<Duration>(_dt).count() * 1000.0f;
            m_stats.physicsTicksPerSecond = 1.0f / std::chrono::duration_cast<Duration>(_dt).count();
        }
    }
}

void PhysicsEngine::syncPhysicsToSwapBuffer() {
    auto& writeBuffer = m_swapBuffer.getWriteBuffer();
    writeBuffer.objectSnapshots.clear();

    m_bodyIdCache.clear();
    m_physicsSystem->GetBodies(m_bodyIdCache);

    const auto& bodyInterface = m_physicsSystem->GetBodyInterface();
    for (const auto& bodyID: m_bodyIdCache) {
        Physics::ObjectSnapshot snapshot{};
        snapshot.bodyID = bodyID;
        bodyInterface.GetPositionAndRotation(
                bodyID,
                snapshot.position,
                snapshot.rotation);
        writeBuffer.objectSnapshots.emplace(bodyID, snapshot);
    }

    // swap buffers
    m_swapBuffer.publish();
}

void PhysicsEngine::executeDispatchedFunctions() {
    std::lock_guard<std::mutex> lk(m_dispatchMutex);
    for (auto& fn: m_persistOnPhysicsThreadFn) {
        fn(*this);
    }

    for (auto& fn: m_dispatchedOnPhysicsThreadFn) {
        fn(*this);
    }
    m_dispatchedOnPhysicsThreadFn.clear();
}

void PhysicsEngine::syncTickIndex(size_t remoteTickIndex, RoundTripTimeMs roundTripTimeMs) {
    double oneWayTimeSec = (roundTripTimeMs / 2.0) / 1000.0;
    size_t latencyTicks = static_cast<size_t>(std::round(oneWayTimeSec / PHYSICS_TIMESTEP.count()));

    size_t newEstimated = remoteTickIndex + latencyTicks;
    size_t current = m_currentTickIndex.load();

    moe::Logger::debug(
            "Syncing physics tick index: Local={}, Remote={}, RTT={}ms, NewEstimated={}",
            current, remoteTickIndex, roundTripTimeMs, newEstimated);

    if (newEstimated < current) {
        // todo: handle rewind case later
        moe::Logger::warn(
                "Received older tick index from server: Local={}, Remote={}, Estimated={}",
                current, remoteTickIndex, newEstimated);
        return;
    }

    if (newEstimated == current) {
        return;
    }

    m_currentTickIndex.store(newEstimated);
}

MOE_END_NAMESPACE