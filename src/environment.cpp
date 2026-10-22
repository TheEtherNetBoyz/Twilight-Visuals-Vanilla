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

#include <chrono>

namespace twilight_visuals::environment {

namespace {
DEFINE_HOOK(&dScnKy_env_light_c::setLight, EnvironmentSetLight);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_bg, EnvironmentSetLightBg);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_actor, EnvironmentSetLightActor);

s16* s_bloomPulse;
bool (*s_isSimFrame)();
bool s_bloomPulseGuardAvailable;
thread_local bool s_restoreBloomPulse;
thread_local s16 s_savedBloomPulse;
thread_local bool s_bloomSimulationFrame = true;
std::chrono::steady_clock::time_point s_lastBloomFrameSample{};
std::chrono::duration<double> s_bloomFrameAccumulator{};

bool fallback_bloom_simulation_frame() {
    constexpr std::chrono::duration<double> simulationPeriod{1.0 / 30.0};
    const auto now = std::chrono::steady_clock::now();
    if (s_lastBloomFrameSample.time_since_epoch().count() == 0) {
        s_lastBloomFrameSample = now;
        return true;
    }
    auto elapsed = now - s_lastBloomFrameSample;
    s_lastBloomFrameSample = now;
    // Treat a pause or loading stall as one simulation step instead of trying
    // to replay a backlog of bloom pulses on the next rendered frame.
    if (elapsed > std::chrono::milliseconds(250)) {
        s_bloomFrameAccumulator = {};
        return true;
    }
    s_bloomFrameAccumulator += elapsed;
    if (s_bloomFrameAccumulator < simulationPeriod) return false;
    do {
        s_bloomFrameAccumulator -= simulationPeriod;
    } while (s_bloomFrameAccumulator >= simulationPeriod);
    return true;
}

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
    s_bloomSimulationFrame = s_isSimFrame != nullptr
        ? s_isSimFrame()
        : fallback_bloom_simulation_frame();
    s_restoreBloomPulse = s_bloomPulseGuardAvailable && active() &&
                          !s_bloomSimulationFrame;
    // Presentation redraws must not advance the shared simulation-owned pulse,
    // regardless of the selected bloom renderer or visual style.
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
    if (hooks != nullptr &&
        hooks->resolve(mod_ctx, "S_fuwan_sin", &address, &flags) == MOD_OK &&
        (flags & HOOK_SYMBOL_DATA)) {
        s_bloomPulse = static_cast<s16*>(address);
    }
    address = nullptr;
    flags = {};
    if (hooks != nullptr &&
        hooks->resolve(mod_ctx, "dusk::game_clock::is_sim_frame", &address, &flags) == MOD_OK &&
        (flags & HOOK_SYMBOL_CODE)) {
        s_isSimFrame = reinterpret_cast<bool (*)()>(address);
    }
    // The shared pulse can still be protected on older hosts by using the
    // 30 Hz fallback when the private simulation-frame query is unavailable.
    s_bloomPulseGuardAvailable = s_bloomPulse != nullptr;

    ModResult result = mods::hook::add_pre<EnvironmentSetLight>(set_light_pre);
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

bool bloom_simulation_frame() {
    return s_bloomSimulationFrame;
}

void uninstall_hooks() {
    if (s_restoreBloomPulse) {
        *s_bloomPulse = s_savedBloomPulse;
        s_restoreBloomPulse = false;
    }
    weather::restore();
    s_bloomPulseGuardAvailable = false;
    s_bloomPulse = nullptr;
    s_isSimFrame = nullptr;
    s_bloomSimulationFrame = true;
    s_lastBloomFrameSample = {};
    s_bloomFrameAccumulator = {};
    mods::hook::uninstall<EnvironmentSetLightActor>();
    mods::hook::uninstall<EnvironmentSetLightBg>();
    mods::hook::uninstall<EnvironmentSetLight>();
}

}  // namespace twilight_visuals::environment
