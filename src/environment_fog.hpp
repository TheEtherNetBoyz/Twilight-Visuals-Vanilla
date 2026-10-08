#pragma once

#include "dolphin/gx.h"

namespace twilight_visuals::environment::fog {
void apply_normal_twilight(GXColorS10& color, float& nearDistance, float& farDistance);
void apply_distance(GXColorS10& color, float& nearDistance, float& farDistance);
}
