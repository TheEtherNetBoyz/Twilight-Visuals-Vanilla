#pragma once
#include "dolphin/types.h"
#include "visual_types.hpp"
namespace twilight_visuals::sky {
bool read(VisualSkybox*, const char*, u8, u8, u8);
bool select_layer(int layer, int minimum);
void shutdown();
}
