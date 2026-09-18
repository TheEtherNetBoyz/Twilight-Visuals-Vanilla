#pragma once
#include "dolphin/types.h"
#include "visual_types.hpp"

namespace twilight_visuals::compat {
float get_master_volume();
bool get_authored_sky(VisualSkybox& outSky, u8 variant);
}  // namespace twilight_visuals::compat
