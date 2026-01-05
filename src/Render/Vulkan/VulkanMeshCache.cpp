#include "Render/Vulkan/VulkanMeshCache.hpp"
#include "Render/Vulkan/VulkanEngine.hpp"
#include "Render/Vulkan/VulkanLoaders.hpp"


namespace moe {
    void VulkanMeshCache::init(VulkanEngine& engine) {
        m_engine = &engine;
        m_initialized = true;

        defaults.rectMeshId = loadMesh(getDefaultRectMesh());
    }

    MeshId VulkanMeshCache::loadMesh(VulkanCPUMesh cpuMesh) {
        MOE_ASSERT(m_initialized, "VulkanMeshCache not initialized");

        auto buffer = m_engine->uploadMesh(cpuMesh.indices, cpuMesh.vertices, cpuMesh.skinningData);
        MeshId id = m_idAllocator.allocateId();
        m_meshes.emplace(
                id,
                VulkanGPUMesh{
                        .gpuBuffer = std::move(buffer),
                        .min = cpuMesh.min,
                        .max = cpuMesh.max,
                });

        return id;
    }

    Optional<VulkanGPUMesh> VulkanMeshCache::getMesh(MeshId id) const {
        MOE_ASSERT(m_initialized, "VulkanMeshCache not initialized");

        auto it = m_meshes.find(id);
        if (it != m_meshes.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void VulkanMeshCache::destroy() {
        for (auto& [id, mesh]: m_meshes) {
            m_engine->destroyBuffer(mesh.gpuBuffer.vertexBuffer);
            m_engine->destroyBuffer(mesh.gpuBuffer.indexBuffer);
            if (mesh.gpuBuffer.hasSkinningData) {
                m_engine->destroyBuffer(mesh.gpuBuffer.skinningDataBuffer);
            }
        }

        m_idAllocator.reset();
        m_meshes.clear();

        m_engine = nullptr;
        m_initialized = false;
    }
}// namespace moe