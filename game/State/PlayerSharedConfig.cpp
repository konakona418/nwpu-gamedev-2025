#include "PlayerSharedConfig.hpp"

namespace game {
    ParamF PlayerConfig::PLAYER_SPEED("player.speed", 6.0f);
    ParamF PlayerConfig::PLAYER_JUMP_VELOCITY("player.jump_velocity", 3.0f);

    ParamF PlayerConfig::PLAYER_MOUSE_SENSITIVITY("player.rotation_speed", 0.1f, ParamScope::UserConfig);

    ParamF PlayerConfig::PLAYER_HALF_HEIGHT("player.half_height", 0.5f);
    ParamF PlayerConfig::PLAYER_RADIUS("player.radius", 0.3f);

    // offset from mass center (half height) to camera position (eye level)
    ParamF PlayerConfig::PLAYER_CAMERA_OFFSET_Y("player.camera_offset_y", 0.6f);
    ParamF PlayerConfig::PLAYER_CAMERA_LERP_FACTOR("player.camera_lerp_factor", 0.1f);

    ParamF PlayerConfig::PLAYER_SUPPORTING_VOLUME_CONSTANT("player.supporting_volume_constant", -0.5f);
    ParamF PlayerConfig::PLAYER_MAX_SLOPE_ANGLE_DEGREES("player.max_slope_angle_degrees", 45.0f);

    ParamF PlayerConfig::PLAYER_STICK_TO_FLOOR_STEP_DOWN("player.stick_to_floor_step_down", -0.4f);
    ParamF PlayerConfig::PLAYER_WALK_STAIRS_STEP_UP("player.walk_stairs_step_up", 0.4f);

    ParamI PlayerConfig::MAX_SIMULTANEOUS_PLAYER_FOOTSTEPS(
            "gameplay.max_simultaneous_local_footsteps",
            4, ParamScope::System);
    ParamF PlayerConfig::PLAYER_FOOTSTEP_SOUND_COOLDOWN(
            "gameplay.player_footstep_sound_cooldown",
            0.5f, ParamScope::System);
}// namespace game