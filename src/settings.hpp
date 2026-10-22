#pragma once

#include "mods/svc/config.h"

namespace twilight_visuals {

struct Settings {
    ConfigVarHandle enabled{};
    ConfigVarHandle visualEffects{};
    ConfigVarHandle twilightCameraLight{};
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
    ConfigVarHandle betterBloomIntensity{};
    ConfigVarHandle bloomSize{};
    ConfigVarHandle bloomThreshold{};
    ConfigVarHandle bloomDiffusion{};
    ConfigVarHandle bloomSaturation{};
    ConfigVarHandle bloomBlendMode{};
    ConfigVarHandle bloomPulseEnabled{};
    ConfigVarHandle bloomPulsePeriod{};
    ConfigVarHandle bloomPulseSize{};
    ConfigVarHandle bloomPulseIntensity{};
    ConfigVarHandle bloomCoreWidth{}, bloomCoreContribution{};
    ConfigVarHandle bloomInnerWidth{}, bloomInnerContribution{};
    ConfigVarHandle bloomOuterWidth{}, bloomOuterContribution{};
    ConfigVarHandle bloomHaloWidth{}, bloomHaloContribution{};
    ConfigVarHandle bloomGainSource{}, bloomResponseGain{}, bloomHighlightRolloff{};
    ConfigVarHandle bloomAutoCompensation{}, bloomHighlightLimit{}, bloomHighlightDesaturation{};
    ConfigVarHandle bloomTintSource{}, bloomTintRed{}, bloomTintGreen{}, bloomTintBlue{};
    ConfigVarHandle bloomColourLayerStrength{}, bloomSceneDarkening{};
    ConfigVarHandle bloomPulseActivation{}, bloomPulseIrregular{}, bloomPulseTransition{};
    ConfigVarHandle bloomThresholdSource{}, bloomThresholdNormalization{};
    ConfigVarHandle bloomActivation{}, bloomResolution{};
    ConfigVarHandle bloomEnabled{}, bloomPreset{}, bloomOverrideThreshold{};
    ConfigVarHandle bloomBrightnessMetric{}, bloomPreDiffusionGain{}, bloomSourceLimit{};
    ConfigVarHandle bloomFilteringSpace{}, bloomHorizontalSpread{}, bloomVerticalSpread{};
    ConfigVarHandle darkHourFogStart{};
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
    ConfigVarHandle hotkeyGyro{};
    ConfigVarHandle hotkeyBloom{};
    ConfigVarHandle hotkeyTextures{};
    ConfigVarHandle hotkeyGyroEnabled{};
    ConfigVarHandle hotkeyTexturesEnabled{};
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
