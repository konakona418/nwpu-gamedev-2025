#pragma once

#include "Render/Vulkan/VulkanIdTypes.hpp"
#include "Render/Vulkan/VulkanTypes.hpp"


namespace moe {
    struct VulkanSceneMesh;

    constexpr size_t INVALID_JOINT_MATRIX_START_INDEX = std::numeric_limits<size_t>::max();

    struct VulkanRenderPacket {
        static constexpr uint32_t INVALID_SORT_KEY = std::numeric_limits<uint32_t>::max();

        MeshId meshId;
        MaterialId materialId;
        glm::mat4 transform;
        uint32_t sortKey{INVALID_SORT_KEY};// todo: add sorting key

        bool skinned{false};
        size_t jointMatrixStartIndex{INVALID_JOINT_MATRIX_START_INDEX};// for skinned meshes

        VkDeviceAddress skinnedVertexBufferAddr{0};// for skinned meshes
    };

    struct VulkanDrawContext {
        const VulkanDrawContext* lastContext{nullptr};
        Vector<VulkanSceneMesh>* sceneMeshes{nullptr};

        size_t jointMatrixStartIndex{INVALID_JOINT_MATRIX_START_INDEX};

        bool isNull() const { return lastContext == nullptr && sceneMeshes == nullptr; }
    };

    const static VulkanDrawContext NULL_DRAW_CONTEXT = {nullptr, nullptr, INVALID_JOINT_MATRIX_START_INDEX};

    enum class VulkanRenderableFeature : uint32_t {
        None = 0,
        HasHierarchy = 1 << 0,
        HasSkeletalAnimation = 1 << 1,
    };

    template<VulkanRenderableFeature... Features>
    bool hasRenderFeature(VulkanRenderableFeature feature) {
        return ((!!((uint32_t) feature & (uint32_t) Features)) && ...);
    }

    inline bool hasRenderFeature(VulkanRenderableFeature feature, Span<VulkanRenderableFeature> features) {
        for (auto f: features) {
            if (!((uint32_t) feature & (uint32_t) f)) {
                return false;
            }
        }
        return true;
    }

    template<VulkanRenderableFeature... Features>
    VulkanRenderableFeature makeRenderFeatureBitmask() {
        return (VulkanRenderableFeature) ((uint32_t) Features | ...);
    }

    inline VulkanRenderableFeature makeRenderFeatureBitmask(Span<VulkanRenderableFeature> features) {
        uint32_t bitmask = 0;
        for (auto feature: features) {
            bitmask |= (uint32_t) feature;
        }
        return (VulkanRenderableFeature) bitmask;
    }

    struct VulkanRenderable {
        virtual ~VulkanRenderable() = default;

        virtual void updateTransform(const glm::mat4& parentTransform) = 0;
        virtual void gatherRenderPackets(Vector<VulkanRenderPacket>& packets, const VulkanDrawContext& drawContext) = 0;

        virtual VulkanRenderableFeature getFeatures() const { return VulkanRenderableFeature::None; }

        virtual void* getFeatureImpl(size_t featureId) { return nullptr; }

        template<typename T>
        T* as() {
            return static_cast<T*>(this->getFeatureImpl(T::FEATURE_ID));
        }

        template<typename T, typename... Features, typename = std::enable_if_t<std::is_same_v<Features..., VulkanRenderableFeature>>>
        Optional<T*> checkedAs(Features... features) {
            Vector<VulkanRenderableFeature> required{features...};
            if (hasRenderFeature(this->getFeatures(), required)) {
                auto feature = as<T>();
                MOE_ASSERT(feature != nullptr, "Feature pointer is null");
                return feature;
            }
            return std::nullopt;
        }

        template<VulkanRenderableFeature... Features>
        bool hasFeature() {
            return hasRenderFeature<Features...>(this->getFeatures());
        }
    };
}// namespace moe
