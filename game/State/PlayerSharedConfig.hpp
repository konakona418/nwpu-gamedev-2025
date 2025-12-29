#pragma once

#include "Param.hpp"

namespace game {
    struct Config {
        static ParamF PLAYER_SPEED;
        static ParamF PLAYER_JUMP_VELOCITY;

        static ParamF PLAYER_MOUSE_SENSITIVITY;

        static ParamF PLAYER_HALF_HEIGHT;
        static ParamF PLAYER_RADIUS;

        // offset from mass center (half height) to camera position (eye level)
        static ParamF PLAYER_CAMERA_OFFSET_Y;
        static ParamF PLAYER_CAMERA_LERP_FACTOR;

        static ParamF PLAYER_SUPPORTING_VOLUME_CONSTANT;
        static ParamF PLAYER_MAX_SLOPE_ANGLE_DEGREES;

        static ParamF PLAYER_STICK_TO_FLOOR_STEP_DOWN;
        static ParamF PLAYER_WALK_STAIRS_STEP_UP;
    };
}// namespace game