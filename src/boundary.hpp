#pragma once
namespace twilight_visuals::boundary {
void initialize();
void shutdown();
void begin_visual_environment();
void end_visual_environment();
void set_native_moon_initialization(bool enabled);
bool native_moon_initialization_active();
}
