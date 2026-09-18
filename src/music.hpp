#pragma once

#include "mods/api.h"
#include <cstdint>

namespace twilight_visuals::music {
ModResult initialize();
void shutdown();
void update();
void set_volume(float value);
void suspend();
void prepare_scene();
void sequence(bool scene, bool eligible, int mode, float gain, bool scope, bool battle,
              float battleVolume, bool boss, float bossVolume,
              std::uint32_t bossMain, std::uint32_t bossSub);
}
