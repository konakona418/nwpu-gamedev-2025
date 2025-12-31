#include "State/WeaponConfig.hpp"

namespace game {
    ParamF WeaponConfig::GLOCK_CYCLE_TIME(
            "weapon.glock.cycle_time",
            0.15f,
            ParamScope::System);
    ParamF WeaponConfig::USP_CYCLE_TIME(
            "weapon.usp.cycle_time",
            0.17f,
            ParamScope::System);
    ParamF WeaponConfig::DEAGLE_CYCLE_TIME(
            "weapon.deagle.cycle_time",
            0.23f,
            ParamScope::System);
    ParamF WeaponConfig::AK47_CYCLE_TIME(
            "weapon.ak47.cycle_time",
            0.10f,
            ParamScope::System);
    ParamF WeaponConfig::M4A1_CYCLE_TIME(
            "weapon.m4a1.cycle_time",
            0.09f,
            ParamScope::System);
}// namespace game