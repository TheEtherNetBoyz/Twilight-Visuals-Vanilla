#include "runtime.hpp"

#include "settings.hpp"
#include "compat.hpp"
#include "music.hpp"

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "f_pc/f_pc_name.h"
#include "Z2AudioLib/Z2SceneMgr.h"

#include <algorithm>
#include <cstring>

namespace twilight_visuals {
namespace {
RuntimeSettings g_runtime;
bool g_speedrunSuppressed = false;
bool g_musicGameplayReady = false;

bool is_palace_music_stage(const char* stage) {
    return stage != nullptr && std::strncmp(stage, "D_MN08", 6) == 0;
}

bool is_temple_music_stage(const char* stage) {
    return stage != nullptr && std::strncmp(stage, "D_MN", 4) == 0 &&
           !is_palace_music_stage(stage);
}

bool custom_music_allowed() {
    // Keep the story sequence that owns Midna's Lament under native control.
    // The save/event container is not guaranteed to exist during startup or a
    // stage teardown.  The caller will retry once gameplay is live again.
    if (dComIfGp_getStage() == nullptr || dComIfGp_getPlayer(0) == nullptr) {
        return false;
    }
    return !dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[104]) ||
           dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[250]);
}
}

const RuntimeSettings& runtime_settings() { return g_runtime; }

void refresh_runtime_settings() {
    const Settings& config = settings();
    g_runtime.enabled = get_bool(config.enabled);
    g_runtime.style = static_cast<Style>(std::clamp<std::int64_t>(get_int(config.style), 0, 3));
    g_runtime.brightness =
        static_cast<float>(std::clamp<std::int64_t>(get_int(config.brightness, 100), 0, 120)) /
        100.0f;
    g_runtime.chromaticAberration =
        static_cast<int>(std::clamp<std::int64_t>(get_int(config.chromaticAberration, 80), 0, 200));
    g_runtime.skybox =
        static_cast<Skybox>(std::clamp<std::int64_t>(get_int(config.skybox), 0, 16));
    g_runtime.weather =
        g_speedrunSuppressed ? Weather::Current :
        static_cast<Weather>(std::clamp<std::int64_t>(get_int(config.weather), 0, 8));
    g_runtime.musicVolume =
        static_cast<float>(std::clamp<std::int64_t>(get_int(config.musicVolume, 100), 0, 100)) /
        100.0f;
    g_runtime.bloomMode = static_cast<BloomMode>(
        std::clamp<std::int64_t>(get_int(config.bloomMode, 0), 0, 3));
    g_runtime.bloomBrightness =
        static_cast<float>(std::clamp<std::int64_t>(
            get_int(config.bloomBrightness, 100), 0, 100)) / 100.0f;
    g_runtime.legacyBloom = get_bool(config.legacyBloom);
    g_runtime.skywardSwordRunning = get_bool(config.skywardSwordRunning);
    g_runtime.skywardSwordWallRunning = get_bool(config.skywardSwordWallRunning);
    g_runtime.overrideTempleMusic = get_bool(config.overrideTempleMusic);
    g_runtime.humanWolfSenses = get_bool(config.humanWolfSenses);
    g_runtime.excludePalaceOfTwilight = get_bool(config.excludePalaceOfTwilight, true);
    g_runtime.hideGameplayCursor = get_bool(config.hideGameplayCursor);
    g_runtime.menuScaling = static_cast<MenuScaling>(
        std::clamp<std::int64_t>(get_int(config.menuScaling, 0), 0, 3));
    g_runtime.faceOverride = get_bool(config.faceOverride);
    g_runtime.faceExpression = static_cast<int>(
        std::clamp<std::int64_t>(get_int(config.faceExpression, 0), 0, 162));
    g_runtime.loadMode = static_cast<LoadMode>(
        std::clamp<std::int64_t>(get_int(config.loadMode, 0), 0, 1));
    // Keep the linkage policy in the mod: the host only exposes its current
    // master multiplier and applies the value sent here to the streamed mix.
    // Do not touch the audio stream parameter before the vanilla stage has
    // created its scene/audio state.  This keeps the settings slider live in
    // gameplay while avoiding the startup/reset null window.
    if (dComIfGp_getStage() != nullptr) {
        music::set_volume(g_runtime.musicVolume * compat::get_master_volume());
    }

    // The global start-stage name survives title/file-select screens. Do not
    // let that stale gameplay name authorize custom music there. Keep the
    // latch across normal room/load-zone gaps, matching the MFB behavior.
    const bool titleOrFileSelect = dComIfGp_getStage() != nullptr &&
                                   fopAcM_SearchByName(fpcNm_TITLE_e) != nullptr;
    if (titleOrFileSelect) {
        g_musicGameplayReady = false;
        music::suspend();
    } else if (!g_musicGameplayReady && dComIfGp_getStage() != nullptr &&
               dComIfGp_getPlayer(0) != nullptr) {
        g_musicGameplayReady = true;
    }
}

void provide_visual_state(u8* enabled, u8* style, f32* brightness,
                          s32* chromaticAberration, u8* skyVariant, u8* weather,
                          u8* alternateRun) {
    if (enabled != nullptr) *enabled = active() ? 1 : 0;
    if (style != nullptr) *style = static_cast<u8>(g_runtime.style);
    if (brightness != nullptr) *brightness = g_runtime.brightness;
    if (chromaticAberration != nullptr) *chromaticAberration = g_runtime.chromaticAberration;
    if (skyVariant != nullptr) *skyVariant = static_cast<u8>(g_runtime.skybox);
    if (weather != nullptr) *weather = static_cast<u8>(g_runtime.weather);
    if (alternateRun != nullptr) *alternateRun = g_runtime.skywardSwordRunning ? 1 : 0;
}

s16 provide_enemy_proc(s16 procName) {
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || stage == nullptr || palace_excluded() ||
        dComIfG_play_c::getLayerNo(0) == 14) {
        return procName;
    }

    switch (procName) {
    case fpcNm_E_RD_e:
        return fpcNm_E_RDY_e;
    case fpcNm_E_BA_e:
        return fpcNm_E_YK_e;
    case fpcNm_E_DB_e:
        return fpcNm_E_YD_e;
    case fpcNm_E_HB_e:
        return fpcNm_E_YH_e;
    case fpcNm_E_KR_e:
        return fpcNm_E_YR_e;
    case fpcNm_E_MS_e:
        return fpcNm_E_YG_e;
    default:
        return procName;
    }
}

s32 provide_environment_layer(s32 currentLayer) {
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || stage == nullptr || palace_excluded()) {
        return -1;
    }
    return 14;
}

u8 provide_bloom_profile(u8 defaultProfile) {
    const char* stage = dComIfGp_getStartStageName();
    return active() && stage && !palace_excluded() &&
        !daPy_py_c::checkNowWolfPowerUp() && g_env_light.field_0x12fc < 0 ? 1 : defaultProfile;
}

bool provide_scene_music(const char* spot, s32 room, s32 layer, s32 sceneNo,
                         bool inDarkness, u8 demoWave, u32* bgmId, u8* bgmWave1,
                         u8* bgmWave2, bool* preserveStreams, bool* fieldBgmPlay,
                         s32* musicStatus) {
    (void)room;
    (void)layer;
    (void)inDarkness;

    // The scene provider can see the stale start-stage name while the title
    // actor is active. Require the same gameplay/next-stage context as MFB.
    if (dComIfGp_getPlayer(0) == nullptr && !dComIfGp_isEnableNextStage()) {
        return false;
    }

    const bool palaceSpot = is_palace_music_stage(spot);
    const bool templeScene = is_temple_music_stage(spot) ||
        sceneNo == Z2SCENE_FOREST_TEMPLE ||
        sceneNo == Z2SCENE_GORON_MINES ||
        sceneNo == Z2SCENE_LAKEBED_TEMPLE ||
        sceneNo == Z2SCENE_ARBITERS_GROUNDS ||
        sceneNo == Z2SCENE_SNOWPEAK_RUINS ||
        sceneNo == Z2SCENE_TEMPLE_OF_TIME ||
        sceneNo == Z2SCENE_CITY_IN_THE_SKY ||
        sceneNo == Z2SCENE_HYRULE_CASTLE;
    const bool palaceScene = sceneNo >= Z2SCENE_PALACE_OF_TWILIGHT &&
                             sceneNo <= Z2SCENE_PALACE_OF_TWILIGHT_BOSS;
    const bool palaceMusicScene = palaceScene || palaceSpot;
    const bool templeMusicScene = templeScene || palaceMusicScene;

    if (!active() || !music_override_allowed() || spot == nullptr ||
        (templeMusicScene && !g_runtime.overrideTempleMusic) ||
        (!palaceSpot && spot[0] != 'F' && spot[0] != 'R' &&
            !(g_runtime.overrideTempleMusic && templeMusicScene)) ||
        (demoWave != 0 && sceneNo != Z2SCENE_KAKARIKO_VILLAGE)) {
        return false;
    }

    if ((g_runtime.excludePalaceOfTwilight && palaceMusicScene) ||
        sceneNo == Z2SCENE_FINAL_BATTLE_CUTSCENE) return false;

    if (bgmId != nullptr) *bgmId = Z2BGM_DUNGEON_LV8;
    if (bgmWave1 != nullptr) *bgmWave1 = 0x28;
    if (bgmWave2 != nullptr) *bgmWave2 = 0;
    if (preserveStreams != nullptr) *preserveStreams = true;
    if (fieldBgmPlay != nullptr) *fieldBgmPlay = false;
    if (musicStatus != nullptr) *musicStatus = spot[0] == 'R' ? 1 : 0;
    return true;
}

bool provide_grass(bool* monochrome) {
    if (monochrome == nullptr) return false;
    const char* stage = dComIfGp_getStartStageName();
    *monochrome = active() && g_runtime.style == Style::BlackAndWhite &&
        stage != nullptr && !palace_excluded();
    return true;
}

bool active() {
    return g_runtime.enabled && !g_speedrunSuppressed;
}

bool palace_excluded() {
    const char* stage = dComIfGp_getStartStageName();
    return g_runtime.excludePalaceOfTwilight && stage != nullptr &&
           std::strncmp(stage, "D_MN08", 6) == 0;
}

bool music_override_allowed() {
    if (!g_musicGameplayReady || !custom_music_allowed()) return false;

    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr && *stage != '\0') {
        if (is_palace_music_stage(stage)) {
            return g_runtime.overrideTempleMusic && !g_runtime.excludePalaceOfTwilight;
        }
        if (is_temple_music_stage(stage)) return g_runtime.overrideTempleMusic;
        return true;
    }

    // No stage name is available during a genuine gameplay load only; the
    // player/next-stage latch still prevents title/file-select replacement.
    return true;
}

void set_speedrun_suppressed(bool suppressed) {
    g_speedrunSuppressed = suppressed;
    refresh_runtime_settings();
}

}  // namespace twilight_visuals
