#pragma once

#include "mods/api.h"

namespace twilight_visuals::environment {
bool dark_hour_indoor();
bool forest_temple_outside_bridge();
bool faron_woods();
bool castle_town();
bool kakariko_village();
bool gerudo_desert();
bool reduced_dark_hour_outdoor();
float reduced_dark_hour_outdoor_scale();
ModResult install_hooks();
void uninstall_hooks();
void area_reloaded();
}
