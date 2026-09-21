#pragma once

#include "mods/api.h"

namespace twilight_visuals::environment {
bool dark_hour_indoor();
bool forest_temple_outside_bridge();
ModResult install_hooks();
void uninstall_hooks();
void area_reloaded();
}
