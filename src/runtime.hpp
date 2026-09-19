#pragma once
#include "dolphin/types.h"
#include <cstdint>

namespace twilight_visuals {
enum class Style : std::int64_t { Normal = 0, BlackAndWhite = 1, AstralPlane = 2, DarkHour = 3 };
enum class Skybox : std::int64_t {
    TwilightDay = 0, TwilightNight, Sunrise, Sunset, Overcast, FaronTwilight,
    EldinTwilight, LanayruTwilight, PalaceOfTwilight, SacredGrove, Snowpeak,
    GerudoDesert, LakeHylia, FishingHole, Ordon, HyruleField, CastleTown,
};
enum class Weather : std::int64_t {
    Current = 0, Clear, Rain, Snow, Lightning, WindStorm, SnowStorm, HeavyFog, BloodRain,
};
enum class BloomMode : std::int64_t {
    Native = 0, Off, Classic, Dusklight,
};
enum class MenuScaling : std::int64_t {
    Native = 0, GameCube = 1, Wii = 2, Dusklight = 3,
};
enum class LoadMode : std::int64_t { Normal = 0, Fast = 1 };
struct RuntimeSettings {
    bool enabled{}; Style style{Style::Normal}; float brightness{1.0f};
    int chromaticAberration{80}; Skybox skybox{Skybox::TwilightDay};
    Weather weather{Weather::Current}; float musicVolume{1.0f};
    BloomMode bloomMode{BloomMode::Native}; float bloomBrightness{1.0f};
    bool legacyBloom{};
    bool overrideTempleMusic{};
    bool skywardSwordRunning{};
    bool skywardSwordWallRunning{};
    bool humanWolfSenses{};
    bool excludePalaceOfTwilight{true};
    bool hideGameplayCursor{};
    MenuScaling menuScaling{MenuScaling::Native};
    bool faceOverride{};
    int faceExpression{};
    LoadMode loadMode{LoadMode::Normal};
};
const RuntimeSettings& runtime_settings();
void refresh_runtime_settings();
void provide_visual_state(u8*, u8*, f32*, s32*, u8*, u8*, u8*);
s16 provide_enemy_proc(s16 procName);
s32 provide_environment_layer(s32 currentLayer);
u8 provide_bloom_profile(u8 defaultProfile);
bool provide_scene_music(const char*, s32, s32, s32, bool, u8, u32*, u8*, u8*, bool*, bool*, s32*);
bool provide_grass(bool* monochrome);
bool palace_excluded();
bool music_override_allowed();
bool active();
void set_speedrun_suppressed(bool suppressed);
}  // namespace twilight_visuals
