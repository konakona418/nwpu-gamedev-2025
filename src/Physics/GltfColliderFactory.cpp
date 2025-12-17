#include "Physics/GltfColliderFactory.hpp"
#include "Core/FileReader.hpp"
#include "Math/Common.hpp"

#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

#include <filesystem>
#include <tiny_gltf.h>

MOE_BEGIN_PHYSICS_NAMESPACE

namespace Details {
    static const String GLTF_POSITIONS_ACCESSOR{"POSITION"};

    static const String GLTF_SAMPLER_PATH_TRANSLATION{"translation"};
    static const String GLTF_SAMPLER_PATH_ROTATION{"rotation"};
    static const String GLTF_SAMPLER_PATH_SCALE{"scale"};

    glm::vec3 cvtTinyGltfVec3(const std::vector<double>& vec) {
        assert(vec.size() == 3);
        return {vec[0], vec[1], vec[2]};
    }

    glm::quat cvtTinyGltfQuat(const std::vector<double>& vec) {
        assert(vec.size() == 4);
        return {
                static_cast<float>(vec[3]),
                static_cast<float>(vec[0]),
                static_cast<float>(vec[1]),
                static_cast<float>(vec[2]),
        };
    }

    std::tuple<glm::vec3, glm::quat, glm::vec3> splitTransform(const glm::mat4& transform) {
        glm::vec3 T;
        glm::quat R;
        glm::vec3 S;

        // translation
        T = glm::vec3(transform[3]);

        // scale
        S.x = glm::length(glm::vec3(transform[0]));
        S.y = glm::length(glm::vec3(transform[1]));
        S.z = glm::length(glm::vec3(transform[2]));

        // rotation
        glm::mat4 rotMat(1.0f);
        rotMat[0] = transform[0] / S.x;
        rotMat[1] = transform[1] / S.y;
        rotMat[2] = transform[2] / S.z;
        R = glm::quat_cast(rotMat);

        return {T, R, S};
    }

    void loadGltfFile(tinygltf::Model& model, std::filesystem::path path, std::filesystem::path parentDir) {
        tinygltf::TinyGLTF loader;
        String err;
        String warn;

        size_t bufSize = 0;
        auto fileBuf = FileReader::s_instance->readFile(path.string(), bufSize);
        if (!fileBuf) {
            Logger::error("Failed to load glTF file: {}", path.string());
            MOE_ASSERT(false, "Failed to load glTF file");
        }

        bool isBinary = path.extension() == ".glb";
        bool success = false;
        if (isBinary) {
            success = loader.LoadBinaryFromMemory(
                    &model, &err, &warn,
                    reinterpret_cast<const unsigned char*>(fileBuf->data()),
                    bufSize, parentDir.string());
        } else {
            success = loader.LoadASCIIFromString(
                    &model, &err, &warn,
                    reinterpret_cast<const char*>(fileBuf->data()),
                    bufSize, parentDir.string());
        }

        if (!warn.empty()) {
            Logger::warn("GLTF loader warning: {}", warn);
        }

        if (!success) {
            Logger::error("Failed to load glTF: {}", err);
            MOE_ASSERT(false, "Failed to load glTF");
        }
    }

    int findAttributeAccessor(const tinygltf::Primitive& primitive, StringView attributeName) {
        for (const auto& [accessorName, accessorID]: primitive.attributes) {
            if (accessorName == attributeName) {
                return accessorID;
            }
        }
        return -1;
    }

    bool hasAccessor(const tinygltf::Primitive& primitive, StringView attributeName) {
        const auto accessorIndex = findAttributeAccessor(primitive, attributeName);
        return accessorIndex != -1;
    }

    template<typename T>
    Span<const T> getPackedBufferSpan(
            const tinygltf::Model& model,
            const tinygltf::Accessor& accessor) {
        const auto& bv = model.bufferViews[accessor.bufferView];
        const int bs = accessor.ByteStride(bv);
        if (bs != sizeof(T)) {
            Logger::error("Accessor buffer is not tightly packed, expected {}, got {}", sizeof(T), bs);
            MOE_ASSERT(false, "Accessor buffer is not tightly packed");
        }

        const auto& buf = model.buffers[bv.buffer];
        const auto* data =
                reinterpret_cast<const T*>(&buf.data.at(0) + bv.byteOffset + accessor.byteOffset);
        return Span<const T>{data, accessor.count};
    }

    template<typename T>
    Span<const T> getPackedBufferSpan(
            const tinygltf::Model& model,
            const tinygltf::Primitive& primitive,
            const StringView attributeName) {
        const auto accessorIndex = findAttributeAccessor(primitive, attributeName);
        assert(accessorIndex != -1 && "Accessor not found");
        const auto& accessor = model.accessors[accessorIndex];
        return getPackedBufferSpan<T>(model, accessor);
    }

    glm::mat4 loadTransform(const tinygltf::Node& node) {
        glm::mat4 transform(1.0f);

        glm::vec3 T(0.0f);
        glm::quat R(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 S(1.0f);

        if (!node.matrix.empty()) {
            Logger::warn("Matrix not supported for ColliderFactory");
            return transform;
        }

        if (!node.scale.empty()) {
            S = cvtTinyGltfVec3(node.scale);
        }

        if (!node.rotation.empty()) {
            R = cvtTinyGltfQuat(node.rotation);
        }

        if (!node.translation.empty()) {
            T = cvtTinyGltfVec3(node.translation);
        }

        transform = glm::translate(glm::mat4(1.0f), T) *
                    glm::toMat4(R) *
                    glm::scale(glm::mat4(1.0f), S);
        return transform;
    }

    bool isMesh(const tinygltf::Node& node) {
        return node.mesh != -1;
    }

    struct NodeData {
        glm::mat4 localTransform = glm::mat4(1.0f);
        glm::mat4 globalTransform = glm::mat4(1.0f);

        int meshIndex = -1;
        Vector<size_t> childrenIndices;
    };

    void loadNodesRecursively(
            Vector<NodeData>& outNodes,
            const tinygltf::Model& model,
            const tinygltf::Node& gltfNode,
            const glm::mat4& parentTransform = glm::mat4(1.0f)) {
        NodeData nodeData{};

        nodeData.localTransform = Details::loadTransform(gltfNode);
        nodeData.globalTransform = parentTransform * nodeData.localTransform;

        if (isMesh(gltfNode)) {
            nodeData.meshIndex = gltfNode.mesh;
        }

        outNodes.push_back(nodeData);
        size_t currentNodeIndex = outNodes.size() - 1;
        for (const auto& childIdx: gltfNode.children) {
            const auto& childNode = model.nodes[childIdx];

            size_t childStartIndex = outNodes.size();
            loadNodesRecursively(
                    outNodes, model, childNode,
                    nodeData.globalTransform);
            outNodes[currentNodeIndex].childrenIndices.push_back(childStartIndex);
        }
    }
}// namespace Details

JPH::Ref<JPH::StaticCompoundShapeSettings> GltfColliderFactory::shapeFromGltf(StringView filePath) {
    tinygltf::Model model;

    std::filesystem::path pathStr{filePath.data()};
    std::filesystem::path parentDir = pathStr.parent_path();

    Details::loadGltfFile(model, pathStr, parentDir);

    struct PerPrimitiveData {
        JPH::VertexList vertices;
        JPH::IndexedTriangleList triangles;
    };

    struct PerMeshData {
        Vector<PerPrimitiveData> primitives;
    };

    Vector<PerMeshData> meshesData;
    meshesData.reserve(model.meshes.size());
    for (const auto& mesh: model.meshes) {
        PerMeshData meshData{};
        meshData.primitives.resize(mesh.primitives.size());
        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
            const auto& primitive = mesh.primitives[pi];

            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                Logger::error(
                        "Only triangle primitives are supported in GltfColliderFactory; "
                        "Other primitives will be ignored. primitive mode: {}",
                        primitive.mode);
                continue;
            }

            // load positions
            JPH::VertexList vertices;
            const auto positions =
                    Details::getPackedBufferSpan<glm::vec3>(
                            model, primitive,
                            Details::GLTF_POSITIONS_ACCESSOR);
            for (const auto& pos: positions) {
                JPH::Float3 vertex;
                vertex = JPH::Float3(pos.x, pos.y, pos.z);
                vertices.push_back(vertex);
            }

            // load indices
            Vector<uint32_t> triangleIndices;
            if (primitive.indices != -1) {
                const auto& indexAccessor = model.accessors[primitive.indices];
                switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        MOE_ASSERT(indexAccessor.type == TINYGLTF_TYPE_SCALAR, "Invalid index accessor type");
                        {
                            const auto indices = Details::getPackedBufferSpan<std::uint32_t>(model, indexAccessor);
                            triangleIndices.assign(indices.begin(), indices.end());
                        }
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        MOE_ASSERT(indexAccessor.type == TINYGLTF_TYPE_SCALAR, "Invalid index accessor type");
                        {
                            const auto indices = Details::getPackedBufferSpan<std::uint16_t>(model, indexAccessor);
                            triangleIndices.assign(indices.begin(), indices.end());
                        }
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        MOE_ASSERT(indexAccessor.type == TINYGLTF_TYPE_SCALAR, "Invalid index accessor type");
                        {
                            const auto indices = Details::getPackedBufferSpan<std::uint8_t>(model, indexAccessor);
                            triangleIndices.assign(indices.begin(), indices.end());
                        }
                        break;
                    default:
                        MOE_ASSERT(false, "Unsupported index component type");
                }
            }

            if (triangleIndices.empty()) {
                // no indices: generate sequential triangles if possible
                if ((positions.size() % 3) != 0) {
                    Logger::error("No indices and POSITION count is not multiple of 3");
                    MOE_ASSERT(false, "Cannot build triangles without indices");
                } else {
                    triangleIndices.reserve(positions.size());
                    for (uint32_t vi = 0; vi < static_cast<uint32_t>(positions.size()); ++vi) {
                        triangleIndices.push_back(vi);
                    };
                }
            }

            if (triangleIndices.size() % 3 != 0) {
                Logger::error("Triangle indices count is not a multiple of 3");
                MOE_ASSERT(false, "Triangle indices count is not a multiple of 3");
            }

            JPH::IndexedTriangleList triangles{};
            triangles.reserve(triangleIndices.size() / 3);
            for (size_t i = 0; i < triangleIndices.size(); i += 3) {
                uint32_t i1 = triangleIndices[i];
                uint32_t i2 = triangleIndices[i + 1];
                uint32_t i3 = triangleIndices[i + 2];
                JPH::IndexedTriangle triangle(i1, i2, i3, 0);
                triangles.push_back(triangle);
            }
            // assign into resized slot
            meshData.primitives[pi].vertices = std::move(vertices);
            meshData.primitives[pi].triangles = std::move(triangles);
        }
        meshesData.push_back(std::move(meshData));
    }

    Vector<JPH::Ref<JPH::StaticCompoundShapeSettings>> meshShapes;
    for (const auto& meshData: meshesData) {
        JPH::Ref<JPH::StaticCompoundShapeSettings> compoundShapeSettings =
                new JPH::StaticCompoundShapeSettings();

        for (const auto& primitiveData: meshData.primitives) {
            JPH::Ref<JPH::MeshShapeSettings> meshShapeSettings =
                    new JPH::MeshShapeSettings(
                            primitiveData.vertices,
                            primitiveData.triangles);

            // mesh primitive itself is not mutable
            // (at least let's assume so here anyway)
            // ヾ(≧▽≦*)o
            compoundShapeSettings->AddShape(
                    JPH::Vec3::sZero(),
                    JPH::Quat::sIdentity(),
                    meshShapeSettings);
        }

        meshShapes.push_back(compoundShapeSettings);
    }

    // load nodes
    Vector<Details::NodeData> nodesData;
    for (const auto& rootNodeIdx: model.scenes[model.defaultScene].nodes) {
        const auto& rootNode = model.nodes[rootNodeIdx];
        Details::loadNodesRecursively(nodesData, model, rootNode);
    }

    JPH::Ref<JPH::StaticCompoundShapeSettings> rootCompoundShapeSettings =
            new JPH::StaticCompoundShapeSettings();

    for (const auto& nodeData: nodesData) {
        if (nodeData.meshIndex != -1) {
            MOE_ASSERT(
                    nodeData.meshIndex < static_cast<int>(meshShapes.size()),
                    "Invalid mesh index in node");

            auto [glmTranslation, glmRotation, glmScale] =
                    Details::splitTransform(nodeData.globalTransform);

            JPH::Quat rotation = JPH::Quat(glmRotation.x, glmRotation.y, glmRotation.z, glmRotation.w).Normalized();
            JPH::Vec3 position = JPH::Vec3(glmTranslation.x, glmTranslation.y, glmTranslation.z);

            JPH::Ref<JPH::ShapeSettings> finalShapeSettings;

            auto scale = JPH::Vec3(glmScale.x, glmScale.y, glmScale.z);
            if (scale != JPH::Vec3{1.0f, 1.0f, 1.0f}) {
                JPH::Ref<JPH::ScaledShapeSettings> scaledShapeSettings =
                        new JPH::ScaledShapeSettings(
                                meshShapes[nodeData.meshIndex],
                                scale);
                finalShapeSettings = scaledShapeSettings;
            } else {
                finalShapeSettings = meshShapes[nodeData.meshIndex];
            }

            rootCompoundShapeSettings->AddShape(
                    position,
                    rotation,
                    finalShapeSettings);
        }
    }
    return rootCompoundShapeSettings;
}


MOE_END_PHYSICS_NAMESPACE