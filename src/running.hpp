#pragma once
class daAlink_c;
namespace twilight_visuals::running {
void initialize();
void shutdown();
bool is_running();
void refresh_run_speed(daAlink_c* player);
}
