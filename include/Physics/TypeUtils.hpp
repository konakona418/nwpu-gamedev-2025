#pragma once

#include "Physics/JoltIncludes.hpp"

#include "Core/Common.hpp"
#include "Math/Common.hpp"

MOE_BEGIN_PHYSICS_NAMESPACE

template<typename JoltT, typename GlmT>
JoltT toJoltType(const GlmT& vec);

template<typename GlmT, typename JoltT>
GlmT fromJoltType(const JoltT& vec);

template<>
inline JPH::Vec3 toJoltType<JPH::Vec3, glm::vec3>(const glm::vec3& vec) {
    return JPH::Vec3(vec.x, vec.y, vec.z);
}

template<>
inline glm::vec3 fromJoltType<glm::vec3, JPH::Vec3>(const JPH::Vec3& vec) {
    return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

template<>
inline JPH::Vec4 toJoltType<JPH::Vec4, glm::vec4>(const glm::vec4& vec) {
    return JPH::Vec4(vec.x, vec.y, vec.z, vec.w);
}

template<>
inline glm::vec4 fromJoltType<glm::vec4, JPH::Vec4>(const JPH::Vec4& vec) {
    return glm::vec4(vec.GetX(), vec.GetY(), vec.GetZ(), vec.GetW());
}

template<>
inline JPH::Quat toJoltType<JPH::Quat, glm::quat>(const glm::quat& quat) {
    return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
}

template<>
inline glm::quat fromJoltType<glm::quat, JPH::Quat>(const JPH::Quat& quat) {
    return glm::quat(quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ());
}

template<>
inline JPH::Mat44 toJoltType<JPH::Mat44, glm::mat4>(const glm::mat4& mat) {
    JPH::Mat44 joltMat;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            joltMat(col, row) = mat[col][row];
        }
    }
    return joltMat;
}

template<>
inline glm::mat4 fromJoltType<glm::mat4, JPH::Mat44>(const JPH::Mat44& mat) {
    glm::mat4 glmMat(1.0f);
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            glmMat[col][row] = mat(col, row);
        }
    }
    return glmMat;
}

MOE_END_PHYSICS_NAMESPACE