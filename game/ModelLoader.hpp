#pragma once

#include "Core/Meta/Generator.hpp"
#include "Core/Meta/TypeTraits.hpp"

#include "Render/Vulkan/VulkanEngine.hpp"


namespace game {
    struct ModelLoaderParam {
        moe::StringView modelVariant{""};

        ModelLoaderParam() = default;
        explicit ModelLoaderParam(moe::StringView variant)
            : modelVariant(variant) {}

        ModelLoaderParam& setModelVariant(moe::StringView variant) {
            modelVariant = variant;
            return *this;
        }
    };

    struct ModelLoader {
    public:
        using value_type = moe::RenderableId;

        ModelLoader(const ModelLoaderParam& param)
            : m_modelVariant(param.modelVariant) {}

        ModelLoader(ModelLoaderParam&& param)
            : m_modelVariant(param.modelVariant) {}


        moe::Optional<value_type> generate() {
            if (m_cachedRenderableId != moe::NULL_RENDERABLE_ID) {
                return m_cachedRenderableId;
            }

            auto renderableId =
                    moe::VulkanEngine::get()
                            .getResourceLoader()
                            .load(moe::Loader::Gltf, m_modelVariant);

            if (renderableId == moe::NULL_RENDERABLE_ID) {
                return std::nullopt;
            }

            m_cachedRenderableId = renderableId;
            return renderableId;
        }

        moe::String paramString() const {
            return fmt::format("model_loader_{}_{}", m_modelVariant, hashCode());
        }

        uint64_t hashCode() const {
            return std::hash<moe::StringView>()(m_modelVariant);
        }

    private:
        moe::StringView m_modelVariant{""};
        moe::RenderableId m_cachedRenderableId{moe::NULL_RENDERABLE_ID};
    };
}// namespace game