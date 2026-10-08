#include "environment_fog.hpp"

#include "environment.hpp"
#include "runtime.hpp"
#include "boundary.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "f_op/f_op_camera_mng.h"

#include <algorithm>

namespace twilight_visuals::environment::fog {
namespace {

bool environment_active() {
    return visual_effects_active() && dComIfGp_getStartStageName() != nullptr &&
           !palace_excluded();
}

bool emulated_normal_twilight() {
    return environment_active() && runtime_settings().style == Style::Normal &&
           dComIfG_play_c::getLayerNo(0) != 14 &&
           !boundary::using_authored_twilight_environment();
}

void adjust_high_camera(float& fogNear, float& fogFar) {
    auto* camera = static_cast<camera_process_class*>(dComIfGp_getCamera(0));
    auto* player = dComIfGp_getLinkPlayer();
    if (camera == nullptr || player == nullptr || fogFar <= 1.0f) return;
    const float height = std::max(0.0f, camera->view.lookat.eye.y - player->current.pos.y);
    const float blend = std::clamp((height - 700.0f) / 2300.0f, 0.0f, 1.0f);
    if (blend <= 0.0f) return;
    const float originalNear = fogNear;
    const float expandedFar = std::max(fogFar, 24000.0f);
    fogFar += (expandedFar - fogFar) * blend;
    const float expandedNear = fogFar * 0.78f;
    fogNear = std::clamp(originalNear + (expandedNear - originalNear) * blend,
                         0.0f, fogFar - 1.0f);
}

void apply_foreground_visibility(float& fogNear, float fogFar) {
    if (fogFar <= 1.0f) return;
    const float visibility = std::clamp(runtime_settings().darkHourFogStart, 0.0f, 4.0f);
    const float currentNear = std::clamp(fogNear, 0.0f, fogFar - 1.0f);
    const float targetRatio = visibility <= 2.0f
        ? 0.70f + (visibility - 1.0f) * 0.20f
        : 0.90f + (visibility - 2.0f) * 0.05f;
    const float targetNear = visibility <= 1.0f
        ? currentNear + (fogFar * 0.70f - currentNear) * visibility
        : fogFar * targetRatio;
    fogNear = std::clamp(targetNear, 0.0f, fogFar - 1.0f);
}

}  // namespace

void apply_normal_twilight(GXColorS10& color, float& fogNear, float& fogFar) {
    color.r = static_cast<s16>(std::clamp((color.r + 42 * 3) / 4, 0, 255));
    color.g = static_cast<s16>(std::clamp((color.g + 58 * 3) / 4, 0, 255));
    color.b = static_cast<s16>(std::clamp((color.b + 74 * 3) / 4, 0, 255));
    fogNear = std::min(fogNear, 1800.0f);
    fogFar = std::min(fogFar, 18000.0f);
}

void apply_distance(GXColorS10& color, float& fogNear, float& fogFar) {
    if (!environment_active()) return;
    if (emulated_normal_twilight()) {
        apply_normal_twilight(color, fogNear, fogFar);
    } else if (runtime_settings().style == Style::AstralPlane) {
        const GXColorS10& ambient = g_env_light.bg_amb_col[0];
        const auto channel = [](s16 fogValue, s16 ambientValue) {
            return static_cast<s16>(std::clamp((fogValue * 3 + ambientValue * 2) / 5, 0, 1023));
        };
        color.r = channel(g_env_light.fog_col.r, ambient.r);
        color.g = channel(g_env_light.fog_col.g, ambient.g);
        color.b = channel(g_env_light.fog_col.b, ambient.b);
        fogFar = std::clamp(fogFar > 100.0f ? fogFar : 9000.0f, 500.0f, 9000.0f);
        const float authoredNear = fogNear > 0.0f ? fogNear : fogFar * 0.20f;
        fogNear = std::clamp(std::min(authoredNear, fogFar * 0.28f), 0.0f, fogFar - 1.0f);
        adjust_high_camera(fogNear, fogFar);
    } else if (runtime_settings().style == Style::DarkHour) {
        const GXColorS10& ambient = g_env_light.bg_amb_col[0];
        const float luma = std::max(0.0f, ambient.r * 0.20f + ambient.g * 0.70f + ambient.b * 0.10f);
        if (dark_hour_indoor()) {
            color.r = static_cast<s16>(std::clamp(luma * 0.12f + 8.0f, 0.0f, 1023.0f));
            color.g = static_cast<s16>(std::clamp(luma * 0.21f + 14.0f, 0.0f, 1023.0f));
            color.b = static_cast<s16>(std::clamp(luma * 0.36f + 24.0f, 0.0f, 1023.0f));
            fogFar = std::clamp(fogFar > 100.0f ? fogFar : 7800.0f, 4500.0f, 9000.0f);
            const float authoredNear = fogNear > 0.0f ? fogNear : fogFar * 0.72f;
            fogNear = std::clamp(std::max(authoredNear, fogFar * 0.72f), 0.0f, fogFar - 1.0f);
            apply_foreground_visibility(fogNear, fogFar);
            return;
        }
        color.r = static_cast<s16>(std::clamp(luma * 0.20f, 0.0f, 1023.0f));
        color.g = static_cast<s16>(std::clamp(luma * 0.90f + 24.0f, 0.0f, 1023.0f));
        color.b = static_cast<s16>(std::clamp(luma * 0.32f, 0.0f, 1023.0f));
        fogFar = std::clamp(fogFar > 100.0f ? fogFar : 7000.0f, 500.0f, 7000.0f);
        const float authoredNear = fogNear > 0.0f ? fogNear : fogFar * 0.18f;
        fogNear = std::clamp(std::min(authoredNear, fogFar * 0.24f), 0.0f, fogFar - 1.0f);
        apply_foreground_visibility(fogNear, fogFar);
        adjust_high_camera(fogNear, fogFar);
    }
    if (runtime_settings().style != Style::DarkHour)
        apply_foreground_visibility(fogNear, fogFar);
}

}  // namespace twilight_visuals::environment::fog
