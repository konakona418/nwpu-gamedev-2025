#pragma once

#include "GameManager.hpp"

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

#include "Render/Common.hpp"

namespace game {
    // this is not an object pool
    // this is a Renderable pool or Model pool
    struct ObjectPool
        : public moe::Meta::NonCopyable<ObjectPool>,
          public moe::Meta::Singleton<ObjectPool> {
    public:
        MOE_SINGLETON(ObjectPool)

        friend struct ObjectPoolGuard;

        moe::RenderableId getPooledRenderable(moe::StringView modelPath);
        void releasePooledRenderable(moe::RenderableId renderableId);

        void registerRenderableToPool(moe::StringView modelPath, size_t desiredInstanceCount = 1, bool allowSharing = false);

        void loadAllRegisteredRenderables(GameManager& ctx);
        bool loadOneRegisteredRenderable(GameManager& ctx);

        size_t getPoolSize() const {
            size_t total = 0;
            for (const auto& [modelPath, entries]: m_pool) {
                total += entries.size();
            }
            return total;
        }

        size_t getPendingLoadCount() const {
            size_t total = 0;
            for (const auto& modelPath: m_pendingLoads) {
                auto it = m_pool.find(modelPath);
                for (const auto& entry: it->second) {
                    if (entry.renderableId == moe::NULL_RENDERABLE_ID) {
                        total++;
                    }
                }
            }
            return total;
        }

    private:
        struct PoolEntry {
            moe::RenderableId renderableId{moe::NULL_RENDERABLE_ID};
            bool isFree{true};
        };

        moe::UnorderedMap<moe::String, moe::Deque<PoolEntry>> m_pool;
        moe::UnorderedSet<moe::String> m_pendingLoads;
        moe::UnorderedMap<moe::String, bool> m_allowSharingMap;

        moe::RenderableId findFreeOrCreateNewRenderable(moe::StringView modelPath);
    };

    struct ObjectPoolGuard {
    public:
        ObjectPoolGuard(moe::StringView modelPath)
            : m_modelPath(modelPath) {
            m_renderableId = ObjectPool::getInstance().getPooledRenderable(modelPath);
        }

        ~ObjectPoolGuard() {
            auto& pool = ObjectPool::getInstance();
            pool.releasePooledRenderable(m_renderableId);
        }

        moe::RenderableId getRenderableId() const {
            return m_renderableId;
        }

    private:
        moe::String m_modelPath;
        moe::RenderableId m_renderableId{moe::NULL_RENDERABLE_ID};
    };

    struct ObjectPoolRegister {
        ObjectPoolRegister(moe::StringView modelPath, size_t desiredInstanceCount = 1, bool allowSharing = false) {
            auto& pool = ObjectPool::getInstance();
            pool.registerRenderableToPool(modelPath, desiredInstanceCount, allowSharing);
        }
    };
}// namespace game