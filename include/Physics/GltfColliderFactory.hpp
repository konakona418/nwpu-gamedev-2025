#pragma once

#include "Core/Common.hpp"

#include "Physics/JoltIncludes.hpp"
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

MOE_BEGIN_PHYSICS_NAMESPACE

struct GltfColliderFactory {
    static JPH::Ref<JPH::StaticCompoundShapeSettings> shapeFromGltf(StringView filePath);
};

MOE_END_PHYSICS_NAMESPACE