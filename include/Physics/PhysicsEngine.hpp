#pragma once

#include "Physics/JoltIncludes.hpp"
#include "Physics/ObjectSnapshot.hpp"

#include "Core/ABuffer.hpp"
#include "Core/Meta/Feature.hpp"

#include <stdarg.h>

MOE_BEGIN_NAMESPACE

namespace Physics::Details {
    static void TraceImpl(const char* inFMT, ...) {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);

        Logger::info("JoltPhysics trace: {}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS

    static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
        Logger::critical("JoltPhysics assertion failed. {}:{}: ({}) {}",
                         inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
        return true;
    };

#endif// JPH_ENABLE_ASSERTS

    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = JPH::ObjectLayer(0);
        static constexpr JPH::ObjectLayer MOVING = JPH::ObjectLayer(1);
        static constexpr JPH::ObjectLayer NUM_LAYERS = JPH::ObjectLayer(2);
    }// namespace Layers

    class ObjectLayerFilterImpl : public JPH::ObjectLayerPairFilter {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
            switch (inObject1) {
                case Layers::NON_MOVING:
                    return inObject2 == Layers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };

    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS(2);
    }// namespace BroadPhaseLayers

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            // Create a mapping table from object to broad phase layer
            m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        virtual JPH::uint GetNumBroadPhaseLayers() const override {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return m_objectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
            switch ((JPH::BroadPhaseLayer::Type) inLayer) {
                case (JPH::BroadPhaseLayer::Type) BroadPhaseLayers::NON_MOVING:
                    return "NON_MOVING";
                case (JPH::BroadPhaseLayer::Type) BroadPhaseLayers::MOVING:
                    return "MOVING";
                default:
                    JPH_ASSERT(false);
                    return "INVALID";
            }
        }
#endif// JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
            switch (inLayer1) {
                case Layers::NON_MOVING:
                    return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };
}// namespace Physics::Details

struct PhysicsEngine : Meta::Singleton<PhysicsEngine> {
public:
    MOE_SINGLETON(PhysicsEngine)

    using Duration = std::chrono::duration<float, std::chrono::seconds::period>;

    static constexpr JPH::uint MAX_BODIES = 1024;
    static constexpr JPH::uint NUM_BODY_MUTEXES = 0;
    static constexpr JPH::uint MAX_BODY_PAIRS = 1024;
    static constexpr JPH::uint MAX_CONTACT_CONSTRAINTS = 1024;

    static constexpr Duration PHYSICS_TIMESTEP = Duration(1.0f / 60.0f);

    struct SwapBuffer {
        UnorderedMap<JPH::BodyID, Physics::ObjectSnapshot> objectSnapshots;

        Optional<Physics::ObjectSnapshot> getSnapshot(JPH::BodyID id) const {
            if (auto it = objectSnapshots.find(id); it != objectSnapshots.end()) {
                return it->second;
            }
            return {};
        }
    };

    void init();
    void destroy();

    void persistOnPhysicsThread(Function<void(PhysicsEngine&)>&& fn) {
        std::lock_guard<std::mutex> lk(m_dispatchMutex);
        m_persistOnPhysicsThreadFn.push_back(std::move(fn));
    }

    template<typename F>
    void persistOnPhysicsThread(F&& fn) {
        persistOnPhysicsThread(Function<void(PhysicsEngine&)>(std::forward<F>(fn)));
    }

    void dispatchOnPhysicsThread(Function<void(PhysicsEngine&)>&& fn) {
        std::lock_guard<std::mutex> lk(m_dispatchMutex);
        m_dispatchedOnPhysicsThreadFn.push_back(std::move(fn));
    }

    template<typename F>
    void dispatchOnPhysicsThread(F&& fn) {
        dispatchOnPhysicsThread(Function<void(PhysicsEngine&)>(std::forward<F>(fn)));
    }

    JPH::PhysicsSystem& getPhysicsSystem() { return *m_physicsSystem; }

    JPH::TempAllocator* getTempAllocator() { return m_tempAllocator.get(); }

    const SwapBuffer& getCurrentRead() const { return m_swapBuffer.getReadBuffer(); }

    // invoke this every frame to update the read buffer
    void updateReadBuffer() { m_swapBuffer.updateReadBuffer(); }

private:
    PhysicsEngine() = default;
    ~PhysicsEngine() = default;

    bool m_initialized{false};

    ABuffer<SwapBuffer> m_swapBuffer;

    std::atomic_bool m_running{false};
    std::thread m_physicsThread;

    std::mutex m_dispatchMutex;
    Vector<Function<void(PhysicsEngine&)>> m_persistOnPhysicsThreadFn;
    Vector<Function<void(PhysicsEngine&)>> m_dispatchedOnPhysicsThreadFn;

    UniquePtr<Physics::Details::BPLayerInterfaceImpl> m_broadPhaseLayerInterface;
    UniquePtr<Physics::Details::ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBroadPhaseLayerFilter;
    UniquePtr<Physics::Details::ObjectLayerFilterImpl> m_objectLayerFilter;

    UniquePtr<JPH::TempAllocatorImpl> m_tempAllocator;
    UniquePtr<JPH::JobSystemThreadPool> m_jobSystem;

    UniquePtr<JPH::PhysicsSystem> m_physicsSystem;

    JPH::BodyIDVector m_bodyIdCache;

    void launchPhysicsThread();

    void mainLoop();
    void syncPhysicsToSwapBuffer();
    void executeDispatchedFunctions();
};

MOE_END_NAMESPACE