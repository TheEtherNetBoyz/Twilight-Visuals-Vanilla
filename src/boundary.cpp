#include "boundary.hpp"
#include "sky.hpp"
#include "environment.hpp"
#include "particles.hpp"
#include "runtime.hpp"
#include "hook_api.hpp"
#include "mods/service.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_stage.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include <algorithm>
#include <cstring>
namespace twilight_visuals::boundary {
namespace {
static int s_visual_environment_layer = -1;
static int s_visual_environment_room = -1;
static char s_visual_environment_stage[16] = {};
static bool s_visual_environment_has_twilight_layer = false;
static int s_visual_environment_loaded_layer = 0;
static bool s_visual_environment_loaded_twilight = false;
static bool s_visual_environment_area_initialized = false;
static bool s_visual_environment_forced = false;
static bool s_visual_enemy_form_context = false;
static bool restoring = false;
static unsigned s_visual_environment_depth = 0;
static bool s_visual_moon_position_saved = false;
static cXyz s_visual_moon_position{};
static bool s_native_moon_initialization = false;
static bool s_midnight_lighting_time_saved = false;
static f32 s_midnight_lighting_real_time = 0.0f;

struct BackgroundLightState {
    dKy_tevstr_c* tev{};
    GXColorS10 ambient{};
    J3DLightInfo lights[6]{};
    bool saved{};
};
static BackgroundLightState s_backgroundLightState;

DEFINE_HOOK(&dScnKy_env_light_c::exeKankyo, EnvironmentExecute);
DEFINE_HOOK(&dScnKy_env_light_c::setDaytime, EnvironmentSetDaytime);
DEFINE_HOOK(&dKy_darkworld_check, NativeDarkworldCheck);
DEFINE_HOOK_SYMBOL("d/d_kankyo.cpp#envcolor_init", void(), EnvironmentColorInit);
DEFINE_HOOK(&dScnKy_env_light_c::settingTevStruct, SettingTevStruct);
DEFINE_HOOK(static_cast<void (dScnKy_env_light_c::*)(J3DModelData*, dKy_tevstr_c*)>(
                &dScnKy_env_light_c::setLightTevColorType), SetLightTevColorType);
DEFINE_HOOK(static_cast<void (dScnKy_env_light_c::*)(J3DModelData*, dKy_tevstr_c*)>(
                &dScnKy_env_light_c::setLightTevColorType_MAJI), SetLightTevColorTypeMaji);
DEFINE_HOOK(&dKy_Global_amb_set, GlobalAmbientSet);
DEFINE_HOOK(&dKy_bg_MAxx_proc, BackgroundMaterialProc);
DEFINE_HOOK(&dKy_SordFlush_set, SwordFlushSet);
DEFINE_HOOK(&dKy_SunMoon_Light_Check, SunMoonLightCheck);
DEFINE_HOOK(&dKy_twilight_camelight_set, TwilightCameraLightSet);

static bool dark_hour_moon_lighting_active() {
    return active() && runtime_settings().style == Style::DarkHour && !palace_excluded();
}

static bool is_palace_stage() {
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr && std::strncmp(stage, "D_MN08", 6) == 0;
}

// MFB feeds this decision through dKy_darkworld_visual_effect_check(), so every
// engine lighting path sees the same Dark Hour state. Vanilla Dusklight does not
// expose that provider, therefore the dKy_darkworld_check hook below is the
// compatibility boundary. Keep it limited to an actual gameplay scene so stale
// stage names cannot affect title screens or cutscenes.
static bool dark_hour_visual_effects_active() {
    const char* stage = dComIfGp_getStartStageName();
    return active() && runtime_settings().style == Style::DarkHour &&
           s_visual_environment_depth != 0 &&
           stage != nullptr && dComIfGp_getStage() != nullptr &&
           fopAcM_SearchByName(fpcNm_TITLE_e) == nullptr && !palace_excluded();
}

static void dKy_reset_visual_environment_patterns() {
    // Match the initialization performed by envcolor_init(). This clears any
    // weather/gather transition that was started while the normal layer was
    // active before selecting the Twilight palette set.
    g_env_light.wether_pat0 = g_env_light.mColpatWeather;
    g_env_light.wether_pat1 = g_env_light.mColpatWeather;
    g_env_light.mColpatPrevGather = 0xFF;
    g_env_light.mColpatCurrGather = 0xFF;
    g_env_light.mColPatBlendGather = -1.0f;
    g_env_light.mColPatMode = 0;
    g_env_light.mColPatModeGather = 0;
}



static void update() {
    const char* stageName = dComIfGp_getStartStageName();
    const int roomNo = dComIfGp_roomControl_getStayNo();
    const bool forceTwilight = !restoring &&
        (provide_environment_layer(s_visual_environment_loaded_layer) == 14);
    int layerNo = s_visual_environment_loaded_layer;
    {
        const s32 providedLayer = restoring ? s_visual_environment_loaded_layer :
            provide_environment_layer(s_visual_environment_loaded_layer);
        if (providedLayer >= 0 && providedLayer < 15) {
            layerNo = providedLayer;
        }
    }

    // A room/load transition can expose the destination layer before the
    // current environment has been destroyed. Do not decode that transient
    // layer into the live scene. The loaded layer is committed by
    // envcolor_init() only when a complete area environment is created.
    if (forceTwilight == s_visual_environment_forced) {
        return;
    }

    const bool wasForced = s_visual_environment_forced;
    s_visual_environment_layer = layerNo;
    s_visual_environment_room = roomNo;
    s_visual_environment_forced = forceTwilight;
    if (stageName != NULL) {
        strncpy(s_visual_environment_stage, stageName, sizeof(s_visual_environment_stage) - 1);
        s_visual_environment_stage[sizeof(s_visual_environment_stage) - 1] = '\0';
    } else {
        s_visual_environment_stage[0] = '\0';
    }
    s_visual_environment_has_twilight_layer = sky::select_layer(layerNo, layerNo == 14 ? 10 : 0);

    if (forceTwilight || wasForced || s_visual_environment_has_twilight_layer) {
        if (dComIfGp_getStageEnvrInfo() != NULL) {
            g_env_light.stage_envr_info = dComIfGp_getStageEnvrInfo();
        }
        if (dComIfGp_getStagePaletteInfo() != NULL) {
            g_env_light.stage_palette_info = dComIfGp_getStagePaletteInfo();
        }
        if (dComIfGp_getStagePselectInfo() != NULL) {
            g_env_light.stage_pselect_info = dComIfGp_getStagePselectInfo();
        }
        if (dComIfGp_getStageVrboxcolInfo() != NULL) {
            g_env_light.stage_vrboxcol_info = dComIfGp_getStageVrboxcolInfo();
        }

        g_env_light.light_init_timer = 1;
        g_env_light.PrevCol = roomNo;
        g_env_light.UseCol = roomNo;
        g_env_light.pat_ratio = 1.0f;
        dKy_reset_visual_environment_patterns();
    }
}


void commit() {
    environment::area_reloaded();
    particles::area_reloaded();
    s_visual_environment_loaded_layer = dComIfG_play_c::getLayerNo(0);
    s_visual_environment_loaded_twilight = dComIfGp_world_dark_get() != 0;
    s_visual_environment_area_initialized = true;
    s_visual_environment_forced = false;
}

HookAction environment_execute_pre(ModContext*, void*, void*, void*) {
    // Keep the visual clock pinned through the draw phase as well as exeKankyo.
    // Restore the last real sample only after Dark Hour is no longer active.
    if (s_midnight_lighting_time_saved && !dark_hour_moon_lighting_active()) {
        g_env_light.daytime = s_midnight_lighting_real_time;
        s_midnight_lighting_time_saved = false;
    }
    update();
    return HOOK_CONTINUE;
}

void environment_set_daytime_post(ModContext*, void*, void*, void*) {
    if (!dark_hour_moon_lighting_active()) return;

    // setDaytime has already advanced and persisted the real game clock and
    // notified audio. From this point through the later draw/TEV passes, expose
    // a stable midnight sample through the environment's visual clock only.
    s_midnight_lighting_real_time = g_env_light.daytime;
    s_midnight_lighting_time_saved = true;
    g_env_light.daytime = 0.0f;
}

void environment_execute_post(ModContext*, void*, void*, void*) {}

HookAction visual_effect_pre(ModContext*, void*, void*, void*) {
    begin_visual_environment();
    return HOOK_CONTINUE;
}

HookAction sun_moon_light_check_pre(ModContext*, void*, void* retval, void*) {
    begin_visual_environment();
    if (retval != nullptr && dark_hour_moon_lighting_active()) {
        // Native darkworld mode suppresses the game's sun/moon light path.
        // Re-enable that vanilla path for Dark Hour so the moon contributes to
        // TEV lighting and the engine's normal light/shadow calculations.
        *static_cast<u8*>(retval) = TRUE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void visual_effect_post(ModContext*, void*, void*, void*) {
    end_visual_environment();
}

HookAction background_material_light_pre(ModContext*, void* args, void*, void*) {
    begin_visual_environment();
    auto* tev = mods::arg<dKy_tevstr_c*>(args, 2);
    if (tev == nullptr || !active() || palace_excluded() || is_palace_stage() ||
        fopAcM_SearchByName(fpcNm_TITLE_e) != nullptr ||
        runtime_settings().style != Style::DarkHour || tev->Type < 32 || tev->Type > 35)
    {
        return HOOK_CONTINUE;
    }

    s_backgroundLightState.tev = tev;
    s_backgroundLightState.ambient = tev->AmbCol;
    for (int i = 0; i < 6; ++i) {
        s_backgroundLightState.lights[i] = *tev->mLights[i].getLightInfo();
    }
    s_backgroundLightState.saved = true;

    // MFB's visual-Twilight path generated the room background while the Twilight material
    // context was globally active. On vanilla hooks, raise only the background TEV energy before
    // material colors are committed so floors and terrain enter bloom without overexposing Link.
    const f32 user = std::clamp(runtime_settings().brightness, 0.0f, 1.2f);
    const bool indoor = environment::dark_hour_indoor();
    const f32 indoorScale = indoor ? 0.58f : 1.0f;
    const f32 redLift = indoor ? 0.42f : 0.42f;
    const f32 greenLift = indoor ? 0.60f : 1.65f;
    const f32 blueLift = indoor ? 0.86f : 0.62f;
    const f32 redLight = indoor ? 0.42f : 0.38f;
    const f32 greenLight = indoor ? 0.60f : 1.45f;
    const f32 blueLight = indoor ? 0.86f : 0.55f;
    // Palace materials start with substantially higher TEV energy than overworld terrain.
    // Normalize that input before applying the same Dark Hour lift used everywhere else.
    const f32 palaceSourceScale = is_palace_stage() ? 0.12f : 1.0f;
    const auto lift = [user, indoorScale, palaceSourceScale, redLift, greenLift, blueLift](GXColorS10& color) {
        const f32 luma = std::max(0.0f, color.r * 0.25f + color.g * 0.65f + color.b * 0.10f);
        color.r = static_cast<s16>(std::clamp(luma * palaceSourceScale * redLift * user * indoorScale, 0.0f, 1023.0f));
        color.g = static_cast<s16>(std::clamp(luma * palaceSourceScale * greenLift * user * indoorScale, 0.0f, 1023.0f));
        color.b = static_cast<s16>(std::clamp(luma * palaceSourceScale * blueLift * user * indoorScale, 0.0f, 1023.0f));
    };
    lift(tev->AmbCol);
    for (int i = 0; i < 6; ++i) {
        auto* info = tev->mLights[i].getLightInfo();
        const f32 luma = info->mColor.r * 0.25f + info->mColor.g * 0.65f +
                         info->mColor.b * 0.10f;
        info->mColor.r = static_cast<u8>(std::clamp(luma * palaceSourceScale * redLight * user * indoorScale, 0.0f, 255.0f));
        info->mColor.g = static_cast<u8>(std::clamp(luma * palaceSourceScale * greenLight * user * indoorScale, 0.0f, 255.0f));
        info->mColor.b = static_cast<u8>(std::clamp(luma * palaceSourceScale * blueLight * user * indoorScale, 0.0f, 255.0f));
    }
    return HOOK_CONTINUE;
}

void background_material_light_post(ModContext*, void*, void*, void*) {
    if (s_backgroundLightState.saved && s_backgroundLightState.tev != nullptr) {
        s_backgroundLightState.tev->AmbCol = s_backgroundLightState.ambient;
        for (int i = 0; i < 6; ++i) {
            *s_backgroundLightState.tev->mLights[i].getLightInfo() =
                s_backgroundLightState.lights[i];
        }
    }
    s_backgroundLightState = {};
    end_visual_environment();
}

HookAction native_darkworld_check_pre(ModContext*, void*, void* retval, void*) {
    // Dark Hour emulates Twilight visually; it must not report native Twilight
    // to gameplay, actor, HUD, or event systems. Return the visual state only
    // while one of our lighting/render hooks owns the environment scope.
    // ForceMoon still bypasses this narrow visual result while allocating the
    // native celestial packet.
    if (s_native_moon_initialization) return HOOK_CONTINUE;
    if (!dark_hour_visual_effects_active())
        return HOOK_CONTINUE;
    *static_cast<u8*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction environment_color_init_pre(ModContext*, void*, void*, void*) {
    commit();
    update();
    begin_visual_environment();
    return HOOK_CONTINUE;
}

void environment_color_init_post(ModContext*, void*, void*, void*) {
    end_visual_environment();
}
}
void initialize() {
    restoring = false;
    commit();
    mods::hook::add_pre<EnvironmentColorInit>(environment_color_init_pre);
    mods::hook::add_post<EnvironmentColorInit>(environment_color_init_post);
    mods::hook::add_pre<EnvironmentExecute>(environment_execute_pre);
    mods::hook::add_post<EnvironmentExecute>(environment_execute_post);
    mods::hook::add_post<EnvironmentSetDaytime>(environment_set_daytime_post);
    mods::hook::add_pre<NativeDarkworldCheck>(native_darkworld_check_pre);
    mods::hook::add_pre<SettingTevStruct>(visual_effect_pre);
    mods::hook::add_post<SettingTevStruct>(visual_effect_post);
    mods::hook::add_pre<SetLightTevColorType>(visual_effect_pre);
    mods::hook::add_post<SetLightTevColorType>(visual_effect_post);
    mods::hook::add_pre<SetLightTevColorTypeMaji>(background_material_light_pre);
    mods::hook::add_post<SetLightTevColorTypeMaji>(background_material_light_post);
    mods::hook::add_pre<GlobalAmbientSet>(visual_effect_pre);
    mods::hook::add_post<GlobalAmbientSet>(visual_effect_post);
    mods::hook::add_pre<BackgroundMaterialProc>(visual_effect_pre);
    mods::hook::add_post<BackgroundMaterialProc>(visual_effect_post);
    mods::hook::add_pre<SwordFlushSet>(visual_effect_pre);
    mods::hook::add_post<SwordFlushSet>(visual_effect_post);
    mods::hook::add_pre<SunMoonLightCheck>(sun_moon_light_check_pre);
    mods::hook::add_post<SunMoonLightCheck>(visual_effect_post);
    mods::hook::add_pre<TwilightCameraLightSet>(visual_effect_pre);
    mods::hook::add_post<TwilightCameraLightSet>(visual_effect_post);
}
void begin_visual_environment() {
    if (s_visual_environment_depth == 0 && dark_hour_moon_lighting_active()) {
        s_visual_moon_position = g_env_light.moon_pos;
        s_visual_moon_position_saved = true;
        // Keep the native lighting position synchronized with the Dark Hour
        // moon billboard position used by geometry.cpp.
        g_env_light.moon_pos.set(-30000.0f, 45000.0f, -65000.0f);
    }
    ++s_visual_environment_depth;
}
void end_visual_environment() {
    if (s_visual_environment_depth != 0) --s_visual_environment_depth;
    if (s_visual_environment_depth == 0 && s_visual_moon_position_saved) {
        g_env_light.moon_pos = s_visual_moon_position;
        s_visual_moon_position_saved = false;
    }
}
void set_native_moon_initialization(bool enabled) {
    s_native_moon_initialization = enabled;
}
bool native_moon_initialization_active() {
    return s_native_moon_initialization;
}
void shutdown() {
    restoring = true;
    if (s_visual_environment_area_initialized && s_visual_environment_forced &&
        dComIfGp_getStage() && dComIfGp_roomControl_getStayNo() >= 0) update();
    s_visual_enemy_form_context = false;
    s_visual_environment_area_initialized = false;
    s_visual_environment_depth = 0;
    s_native_moon_initialization = false;
    if (s_midnight_lighting_time_saved)
        g_env_light.daytime = s_midnight_lighting_real_time;
    s_midnight_lighting_time_saved = false;
    mods::hook::uninstall<EnvironmentSetDaytime>();
    mods::hook::uninstall<TwilightCameraLightSet>();
    mods::hook::uninstall<SunMoonLightCheck>();
    mods::hook::uninstall<SwordFlushSet>();
    mods::hook::uninstall<BackgroundMaterialProc>();
    mods::hook::uninstall<GlobalAmbientSet>();
    mods::hook::uninstall<SetLightTevColorTypeMaji>();
    mods::hook::uninstall<SetLightTevColorType>();
    mods::hook::uninstall<SettingTevStruct>();
    mods::hook::uninstall<NativeDarkworldCheck>();
    mods::hook::uninstall<EnvironmentExecute>();
    mods::hook::uninstall<EnvironmentColorInit>();
}
}
