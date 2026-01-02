#include "ObjectPool.hpp"

namespace game {
    moe::RenderableId ObjectPool::getPooledRenderable(moe::StringView modelPath) {
        return findFreeOrCreateNewRenderable(modelPath);
    }

    void ObjectPool::releasePooledRenderable(moe::RenderableId renderableId) {
        moe::Logger::debug("Releasing renderable id {} back to pool", renderableId);

        for (auto& [modelPath, entries]: m_pool) {
            for (auto& entry: entries) {
                if (entry.renderableId == renderableId) {
                    entry.isFree = true;
                    return;
                }
            }
        }
    }

    void ObjectPool::registerRenderableToPool(moe::StringView modelPath, size_t desiredInstanceCount, bool allowSharing) {
        if (m_pool.find(modelPath.data()) == m_pool.end()) {
            m_pool[modelPath.data()] = moe::Deque<PoolEntry>{};
            m_pendingLoads.emplace(modelPath.data());
            m_allowSharingMap[modelPath.data()] = allowSharing;
        }

        moe::Logger::info(
                "Registering renderable '{}' to pool with desired instance count {}",
                modelPath,
                desiredInstanceCount);

        auto& entries = m_pool[modelPath.data()];
        entries.resize(desiredInstanceCount);
    }

    void ObjectPool::loadAllRegisteredRenderables(GameManager& ctx) {
        for (auto& [modelPath, entries]: m_pool) {
            moe::Logger::info("Loading renderables for model path '{}'", modelPath);

            for (auto& entry: entries) {
                if (entry.renderableId == moe::NULL_RENDERABLE_ID) {
                    entry.renderableId = ctx.renderer().getResourceLoader().load(moe::Loader::Gltf, moe::asset(modelPath));
                }
            }
        }
        m_pendingLoads.clear();
    }

    bool ObjectPool::loadOneRegisteredRenderable(GameManager& ctx) {
        if (m_pendingLoads.empty()) {
            return false;
        }

        auto it = m_pendingLoads.begin();
        moe::String modelPath = *it;

        moe::Logger::info("Loading renderables for model path '{}'", modelPath);

        auto& entries = m_pool[modelPath];
        for (auto& entry: entries) {
            if (entry.renderableId == moe::NULL_RENDERABLE_ID) {
                entry.renderableId = ctx.renderer().getResourceLoader().load(moe::Loader::Gltf, moe::asset(modelPath));
                return true;
            }
        }

        if (std::all_of(entries.begin(), entries.end(),
                        [](const PoolEntry& entry) {
                            return entry.renderableId != moe::NULL_RENDERABLE_ID;
                        })) {
            m_pendingLoads.erase(it);
        }

        return true;
    }

    moe::RenderableId ObjectPool::findFreeOrCreateNewRenderable(moe::StringView modelPath) {
        auto& entries = m_pool[modelPath.data()];

        bool allowSharing = m_allowSharingMap[modelPath.data()];
        if (allowSharing && !entries.empty()) {
            moe::Logger::debug("Sharing renderable id {} for model path '{}'", entries.front().renderableId, modelPath);
            return entries.front().renderableId;
        }

        for (auto& entry: entries) {
            if (entry.isFree) {
                entry.isFree = false;
                moe::Logger::debug("Allocating renderable id {} from pool for model path '{}'", entry.renderableId, modelPath);
                return entry.renderableId;
            }
        }

        // no free entry, create a new one
        moe::RenderableId newRenderableId = moe::NULL_RENDERABLE_ID;
        if (!entries.empty()) {
            newRenderableId = entries.front().renderableId;
        } else {
            newRenderableId = moe::NULL_RENDERABLE_ID;
        }

        if (newRenderableId == moe::NULL_RENDERABLE_ID) {
            // load the renderable if not loaded yet
            newRenderableId = moe::VulkanEngine::get()
                                      .getResourceLoader()
                                      .load(moe::Loader::Gltf, moe::asset(modelPath));
        }

        moe::Logger::debug("Creating new renderable id {} for model path '{}'", newRenderableId, modelPath);

        PoolEntry newEntry;
        newEntry.renderableId = newRenderableId;
        newEntry.isFree = false;
        entries.push_back(newEntry);

        return newRenderableId;
    }
}// namespace game