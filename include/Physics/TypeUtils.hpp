#pragma once

#include "Physics/JoltIncludes.hpp"

#include "Core/Common.hpp"
#include "Math/Common.hpp"

MOE_BEGIN_PHYSICS_NAMESPACE

template<typename GlmT, typename JoltT>
JoltT toJoltType(const GlmT& vec);

template<typename JoltT, typename GlmT>
GlmT fromJoltType(const JoltT& vec);

template<>
inline JPH::Vec3 toJoltType<const glm::vec3, JPH::Vec3>(const glm::vec3& vec) {
    return JPH::Vec3(vec.x, vec.y, vec.z);
}

template<>
inline glm::vec3 fromJoltType<const JPH::Vec3, glm::vec3>(const JPH::Vec3& vec) {
    return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

template<>
inline JPH::Vec4 toJoltType<const glm::vec4, JPH::Vec4>(const glm::vec4& vec) {
    return JPH::Vec4(vec.x, vec.y, vec.z, vec.w);
}

template<>
inline glm::vec4 fromJoltType<const JPH::Vec4, glm::vec4>(const JPH::Vec4& vec) {
    return glm::vec4(vec.GetX(), vec.GetY(), vec.GetZ(), vec.GetW());
}

template<>
inline JPH::Quat toJoltType<const glm::quat, JPH::Quat>(const glm::quat& quat) {
    return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
}

template<>
inline glm::quat fromJoltType<const JPH::Quat, glm::quat>(const JPH::Quat& quat) {
    return glm::quat(quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ());
}

template<>
inline JPH::Mat44 toJoltType<const glm::mat4, JPH::Mat44>(const glm::mat4& mat) {
    JPH::Mat44 joltMat;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            joltMat(col, row) = mat[col][row];
        }
    }
    return joltMat;
}

template<>
inline glm::mat4 fromJoltType<const JPH::Mat44, glm::mat4>(const JPH::Mat44& mat) {
    glm::mat4 glmMat(1.0f);
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            glmMat[col][row] = mat(col, row);
        }
    }
    return glmMat;
}

MOE_END_PHYSICS_NAMESPACE