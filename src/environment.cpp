#include "environment.hpp"
#include "environment_bloom.hpp"
#include "environment_fog.hpp"
#include "environment_palette.hpp"
#include "environment_skybox.hpp"
#include "environment_weather.hpp"

#include "boundary.hpp"
#include "runtime.hpp"

#include "mods/service.hpp"
#include "hook_api.hpp"

#include "d/d_kankyo.h"

namespace twilight_visuals::environment {

namespace {
DEFINE_HOOK(&dScnKy_env_light_c::setLight, EnvironmentSetLight);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_bg, EnvironmentSetLightBg);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_actor, EnvironmentSetLightActor);

s16* s_bloomPulse;
bool (*s_isSimFrame)();
thread_local bool s_restoreBloomPulse;
thread_local s16 s_savedBloomPulse;

void set_light_post(ModContext*, void* args, void*, void*) {
    if (s_restoreBloomPulse) {
        *s_bloomPulse = s_savedBloomPulse;
        s_restoreBloomPulse = false;
    }
    if (!palette::active()) return;
    auto* env = mods::arg<dScnKy_env_light_c*>(args, 0);
    palette::apply_scene_base(*env);
    skybox::apply(*env);
    bloom::apply_profile();
    palette::finish_scene(*env);
    boundary::end_visual_environment();
}

HookAction set_light_pre(ModContext*, void*, void*, void*) {
    if (palette::active()) boundary::begin_visual_environment();
    weather::apply();
    s_restoreBloomPulse = visual_effects_active() &&
        runtime_settings().bloomMode == BloomMode::ClassicSmoothed &&
        !s_isSimFrame();
    // Presentation redraws must not advance Classic's simulation-owned pulse.
    if (s_restoreBloomPulse) s_savedBloomPulse = *s_bloomPulse;
    return HOOK_CONTINUE;
}

HookAction generated_light_pre(ModContext*, void*, void*, void*) {
    if (palette::active()) boundary::begin_visual_environment();
    return HOOK_CONTINUE;
}

void set_light_bg_post(ModContext*, void* args, void*, void*) {
    if (!palette::active()) return;
    auto* tev = mods::arg<dKy_tevstr_c*>(args, 1);
    auto* colors = mods::arg<GXColorS10*>(args, 2);
    auto* fog = mods::arg<GXColorS10*>(args, 3);
    auto* fogNear = mods::arg<float*>(args, 4);
    auto* fogFar = mods::arg<float*>(args, 5);
    fog::apply_distance(*fog, *fogNear, *fogFar);
    palette::apply_background(*tev, colors, *fog);
    boundary::end_visual_environment();
}

void set_light_actor_post(ModContext*, void* args, void*, void*) {
    if (!palette::active()) return;
    auto* tev = mods::arg<dKy_tevstr_c*>(args, 1);
    auto* fog = mods::arg<GXColorS10*>(args, 2);
    auto* fogNear = mods::arg<float*>(args, 3);
    auto* fogFar = mods::arg<float*>(args, 4);
    fog::apply_distance(*fog, *fogNear, *fogFar);
    palette::apply_actor(*tev, *fog);
    boundary::end_visual_environment();
}
}  // namespace

ModResult install_hooks() {
    void* address = nullptr;
    HookSymbolFlags flags{};
    const auto* hooks = twilight_hook_service();
    ModResult result = hooks->resolve(mod_ctx, "S_fuwan_sin", &address, &flags);
    if (result != MOD_OK) return result;
    if (!(flags & HOOK_SYMBOL_DATA)) return MOD_UNAVAILABLE;
    s_bloomPulse = static_cast<s16*>(address);
    result = hooks->resolve(mod_ctx, "dusk::game_clock::is_sim_frame", &address, &flags);
    if (result != MOD_OK) return result;
    if (!(flags & HOOK_SYMBOL_CODE)) return MOD_UNAVAILABLE;
    s_isSimFrame = reinterpret_cast<bool (*)()>(address);

    result = mods::hook::add_pre<EnvironmentSetLight>(set_light_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<EnvironmentSetLight>(set_light_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<EnvironmentSetLightBg>(generated_light_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<EnvironmentSetLightBg>(set_light_bg_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<EnvironmentSetLightActor>(generated_light_pre);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<EnvironmentSetLightActor>(set_light_actor_post);
}

void area_reloaded() {
    weather::area_reloaded();
}

void uninstall_hooks() {
    if (s_restoreBloomPulse) {
        *s_bloomPulse = s_savedBloomPulse;
        s_restoreBloomPulse = false;
    }
    weather::restore();
    mods::hook::uninstall<EnvironmentSetLightActor>();
    mods::hook::uninstall<EnvironmentSetLightBg>();
    mods::hook::uninstall<EnvironmentSetLight>();
}

}  // namespace twilight_visuals::environment
