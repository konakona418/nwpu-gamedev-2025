#include "State/HudSharedConfig.hpp"

namespace game {
    ParamF4 HudConfig::HUD_TERRORIST_TINT_COLOR("hud.tint.terrorist", ParamFloat4{0.88f, 0.70f, 0.30f, 0.80f});
    ParamF4 HudConfig::HUD_COUNTERTERRORIST_TINT_COLOR("hud.tint.counterterrorist", ParamFloat4{0.40f, 0.60f, 0.90f, 0.80f});
    ParamF HudConfig::HUD_TINT_COLOR_MULTIPLIER("hud.tint.multiplier", 0.5f);
}// namespace game