#pragma once

#include "Physics/JoltIncludes.hpp"

#include "Core/Common.hpp"

MOE_BEGIN_PHYSICS_NAMESPACE

struct ObjectSnapshot {
    JPH::BodyID bodyID;

    JPH::Vec3 position;
    JPH::Quat rotation;
};

MOE_END_PHYSICS_NAMESPACE