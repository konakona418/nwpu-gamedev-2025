#pragma once

#include "Param.hpp"

namespace game {
    inline static ParamF PLAYER_SPEED("player.speed", 3.0f);
    inline static ParamF PLAYER_JUMP_VELOCITY("player.jump_velocity", 5.0f);

    inline static ParamF PLAYER_MOUSE_SENSITIVITY("player.rotation_speed", 0.1f, ParamScope::UserConfig);

    inline static ParamF PLAYER_HALF_HEIGHT("player.half_height", 0.8f);
    inline static ParamF PLAYER_RADIUS("player.radius", 0.4f);

    // offset from mass center (half height) to camera position (eye level)
    inline static ParamF PLAYER_CAMERA_OFFSET_Y("player.camera_offset_y", 0.6f);

    inline static ParamF PLAYER_SUPPORTING_VOLUME_CONSTANT("player.supporting_volume_constant", -0.5f);
    inline static ParamF PLAYER_MAX_SLOPE_ANGLE_DEGREES("player.max_slope_angle_degrees", 45.0f);

    inline static ParamF PLAYER_STICK_TO_FLOOR_STEP_DOWN("player.stick_to_floor_step_down", -0.4f);
    inline static ParamF PLAYER_WALK_STAIRS_STEP_UP("player.walk_stairs_step_up", 0.4f);
}// namespace game