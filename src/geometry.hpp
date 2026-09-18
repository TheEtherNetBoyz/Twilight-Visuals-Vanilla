#pragma once
#include "dolphin/types.h"
struct cXyz;
namespace twilight_visuals::geometry {
void transform_particle(cXyz*, cXyz*, void*, u32, f32);
bool initialize();
void shutdown();
}
