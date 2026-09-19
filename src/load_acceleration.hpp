#pragma once

#include "mods/api.h"

namespace twilight_visuals::load_acceleration {
ModResult install_hooks();
void uninstall_hooks();
void update();
}  // namespace twilight_visuals::load_acceleration
