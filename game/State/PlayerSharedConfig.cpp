#include "PlayerSharedConfig.hpp"

namespace game {
    ParamF Config::PLAYER_SPEED("player.speed", 3.0f);
    ParamF Config::PLAYER_JUMP_VELOCITY("player.jump_velocity", 5.0f);

    ParamF Config::PLAYER_MOUSE_SENSITIVITY("player.rotation_speed", 0.1f, ParamScope::UserConfig);

    ParamF Config::PLAYER_HALF_HEIGHT("player.half_height", 0.8f);
    ParamF Config::PLAYER_RADIUS("player.radius", 0.4f);

    // offset from mass center (half height) to camera position (eye level)
    ParamF Config::PLAYER_CAMERA_OFFSET_Y("player.camera_offset_y", 0.6f);
    ParamF Config::PLAYER_SUPPORTING_VOLUME_CONSTANT("player.supporting_volume_constant", -0.5f);
    ParamF Config::PLAYER_MAX_SLOPE_ANGLE_DEGREES("player.max_slope_angle_degrees", 45.0f);

    ParamF Config::PLAYER_STICK_TO_FLOOR_STEP_DOWN("player.stick_to_floor_step_down", -0.4f);
    ParamF Config::PLAYER_WALK_STAIRS_STEP_UP("player.walk_stairs_step_up", 0.4f);
}// namespace game