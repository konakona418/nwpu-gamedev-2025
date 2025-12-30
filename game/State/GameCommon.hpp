#pragma once

namespace game::State {
    enum class WeaponItems {
        None,
        Glock,
        USP,
        DesertEagle,
        AK47,
        M4A1,
    };

    enum class BombSite {
        A = 0,
        B = 1,
    };

#define _GAME_STATE_WEAPON_ITEM_NAME_ENUM_MAP_XXX()                                                                    \
    X("Glock", ::game::State::WeaponItems::Glock, ::myu::net::Weapon::GLOCK, ::moe::net::Weapon::GLOCK)                \
    X("USP", ::game::State::WeaponItems::USP, ::myu::net::Weapon::USP, ::moe::net::Weapon::USP)                        \
    X("Desert Eagle", ::game::State::WeaponItems::DesertEagle, ::myu::net::Weapon::DEAGLE, ::moe::net::Weapon::DEAGLE) \
    X("AK47", ::game::State::WeaponItems::AK47, ::myu::net::Weapon::AK47, ::moe::net::Weapon::AK47)                    \
    X("M4A1", ::game::State::WeaponItems::M4A1, ::myu::net::Weapon::M4A1, ::moe::net::Weapon::M4A1)
}// namespace game::State