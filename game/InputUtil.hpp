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

        // this function assumes that the `up` vector is always (0, 1, 0)
        // this function returns:
        //   - on xOz plane, a normalized vector representing the horizontal movement direction
        //   - on y axis, either 1, -1 or 0 representing vertical
        // this is designed for FPS style movement controls
        glm::vec3 realMovementVecFPS(const glm::vec3& front, const glm::vec3& right, const glm::vec3& up) {
            glm::vec3 move(0.0f);
            if (this->forward) move += front;
            if (this->backward) move -= front;
            if (this->left) move -= right;
            if (this->right) move += right;

            glm::vec3 xoz = {move.x, 0.0f, move.z};
            float len2 = glm::length2(xoz);
            if (len2 > 0.0001f) {
                xoz = glm::normalize(xoz);
            }

            float vertical = 0.0f;
            if (this->up) vertical += 1.0f;
            if (this->down) vertical -= 1.0f;

            return {xoz.x, vertical, xoz.z};
        }
    };
}// namespace game