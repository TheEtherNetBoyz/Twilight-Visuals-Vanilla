#pragma once

#include "mods/api.h"
#include "mods/svc/ui.h"

#include <string>

namespace twilight_visuals::hotkeys {

enum class Action : uint8_t { Gyro, Bloom, Textures };

ModResult initialize();
void update();
void shutdown();
void begin_binding(Action action, UiElementHandle control);
std::string binding_label(Action action);

}  // namespace twilight_visuals::hotkeys
