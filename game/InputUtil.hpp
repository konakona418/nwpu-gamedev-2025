#pragma once

#include "Core/Common.hpp"

#include "Math/Common.hpp"

namespace game {
    struct MovementHelper {
    public:
        uint32_t forward : 1;
        uint32_t backward : 1;
        uint32_t left : 1;
        uint32_t right : 1;
        uint32_t up : 1;
        uint32_t down : 1;

        MovementHelper() : forward(0), backward(0), left(0), right(0), up(0), down(0) {}

        void resetState() {
            forward = 0;
            backward = 0;
            left = 0;
            right = 0;
            up = 0;
            down = 0;
        }

        glm::vec3 absoluteMovementVec() const {
            glm::vec3 movementVector(0.0f, 0.0f, 0.0f);

            if (forward) movementVector.x += 1.0f;
            if (backward) movementVector.x -= 1.0f;
            if (left) movementVector.z -= 1.0f;
            if (right) movementVector.z += 1.0f;
            if (up) movementVector.y += 1.0f;
            if (down) movementVector.y -= 1.0f;

            // ! handle case where no movement is detected
            // normalizing zero vector is UB
            if (movementVector == glm::vec3(0.0f, 0.0f, 0.0f)) {
                return glm::vec3(0.0f, 0.0f, 0.0f);
            }

            return glm::normalize(movementVector);
        }

        glm::vec3 realMovementVec(const glm::vec3& front, const glm::vec3& right, const glm::vec3& up) {
            glm::vec3 movementVector(0.0f, 0.0f, 0.0f);

            if (this->forward) movementVector += front;
            if (this->backward) movementVector -= front;
            if (this->left) movementVector -= right;
            if (this->right) movementVector += right;
            if (this->up) movementVector += up;
            if (this->down) movementVector -= up;

            if (movementVector == glm::vec3(0.0f, 0.0f, 0.0f)) {
                return glm::vec3(0.0f, 0.0f, 0.0f);
            }

            return movementVector;
        }
    };
}// namespace game