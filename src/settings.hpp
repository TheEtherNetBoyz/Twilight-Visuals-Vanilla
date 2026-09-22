#pragma once

#include "mods/svc/config.h"

namespace twilight_visuals {

struct Settings {
    ConfigVarHandle enabled{};
    ConfigVarHandle style{};
    ConfigVarHandle brightness{};
    ConfigVarHandle perAreaBrightness{};
    ConfigVarHandle currentAreaBrightness{};
    ConfigVarHandle chromaticAberration{};
    ConfigVarHandle skybox{};
    ConfigVarHandle weather{};
    ConfigVarHandle musicVolume{};
    ConfigVarHandle bloomMode{};
    ConfigVarHandle bloomBrightness{};
    ConfigVarHandle legacyBloom{};
    ConfigVarHandle overrideTempleMusic{};
    ConfigVarHandle skywardSwordRunning{};
    ConfigVarHandle sheathSwordWhileSprinting{};
    ConfigVarHandle skywardSwordWallRunning{};
    ConfigVarHandle humanWolfSenses{};
    ConfigVarHandle excludePalaceOfTwilight{};
    ConfigVarHandle hideGameplayCursor{};
    ConfigVarHandle menuScaling{};
    ConfigVarHandle faceOverride{};
    ConfigVarHandle faceExpression{};
    ConfigVarHandle loadMode{};
};

Settings& settings();
ModResult register_settings(ModError* error);
ModResult register_quick_menu_tab(ModError* error);
void unregister_quick_menu_tab();
void close_settings_window();

bool get_bool(ConfigVarHandle handle, bool fallback = false);
int64_t get_int(ConfigVarHandle handle, int64_t fallback = 0);
int64_t current_area_brightness_percent();

}  // namespace twilight_visuals
