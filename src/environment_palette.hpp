#pragma once

#include "d/d_kankyo.h"

namespace twilight_visuals::environment::palette {

bool active();
void apply_scene_base(dScnKy_env_light_c& env);
void finish_scene(dScnKy_env_light_c& env);
void apply_background(dKy_tevstr_c& tev, GXColorS10* colors, GXColorS10& fog);
void apply_actor(dKy_tevstr_c& tev, GXColorS10& fog);

}  // namespace twilight_visuals::environment::palette
