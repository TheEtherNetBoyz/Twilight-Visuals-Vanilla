#pragma once
#include "d/d_kankyo.h"
#include "d/d_kankyo_rain.h"
namespace twilight_visuals::native_particles {
void move(f32 timeScale);
void draw(Mtx, u8**, dKankyo_housi_Packet*, f32 timeScale);
}
