#include "environment.hpp"
#include "environment_bloom.hpp"
#include "environment_fog.hpp"
#include "environment_palette.hpp"
#include "environment_skybox.hpp"
#include "environment_weather.hpp"

#include "boundary.hpp"

#include "mods/service.hpp"
#include "hook_api.hpp"

#include "d/d_kankyo.h"

namespace twilight_visuals::environment {

namespace {
DEFINE_HOOK(&dScnKy_env_light_c::setLight, EnvironmentSetLight);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_bg, EnvironmentSetLightBg);
DEFINE_HOOK(&dScnKy_env_light_c::setLight_actor, EnvironmentSetLightActor);



void set_light_post(ModContext*, void* args, void*, void*) {
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

void uninstall_hooks() {
    weather::restore();
    mods::hook::uninstall<EnvironmentSetLightActor>();
    mods::hook::uninstall<EnvironmentSetLightBg>();
    mods::hook::uninstall<EnvironmentSetLight>();
}

}  // namespace twilight_visuals::environment
