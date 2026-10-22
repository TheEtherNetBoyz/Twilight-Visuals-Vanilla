#pragma once

#include "mods/api.h"
#include "mods/svc/hook.h"

namespace twilight_visuals::better_bloom {
ModResult initialize(ModError* error);
void update();
void shutdown();
HookAction draw(void* bloomObject);
}
