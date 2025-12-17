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

    while (m_running.load()) {
        m_physicsSystem->Update(PHYSICS_TIMESTEP.count(), 1, m_tempAllocator.get(), m_jobSystem.get());

        syncPhysicsToSwapBuffer();
        executeDispatchedFunctions();

        nextTick += step;

        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto remaining = nextTick - now;

            if (remaining > std::chrono::milliseconds(2)) {
                // pretty long time to wait, sleep for 1ms
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else if (remaining > std::chrono::nanoseconds(0)) {
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

MOE_END_NAMESPACE