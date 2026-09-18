#pragma once
#include <mtx.h>

// Vanilla Dusklight has no MFB interpolation bridge.
namespace dusk::frame_interp {
inline float get_interpolation_step() { return 1.0f; }
inline bool is_enabled() { return false; }
inline bool is_sim_frame() { return true; }
inline void record_final_mtx(Mtx, const void*) {}
inline bool lookup_replacement(const void*, Mtx) { return false; }
}
