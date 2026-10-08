#include "environment_skybox.hpp"

#include "compat.hpp"
#include "runtime.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"

#include <algorithm>

namespace twilight_visuals::environment::skybox {
namespace {

void tint_for_style(GXColorS10& color, bool astralAccent = false) {
    const float luma = color.r * 0.25f + color.g * 0.65f + color.b * 0.10f;
    if (runtime_settings().style == Style::DarkHour) {
        color.r = static_cast<s16>(std::clamp(luma * 0.28f, 0.0f, 255.0f));
        color.g = static_cast<s16>(std::clamp(luma * 1.08f, 0.0f, 255.0f));
        color.b = static_cast<s16>(std::clamp(luma * 0.40f, 0.0f, 255.0f));
    } else if (runtime_settings().style == Style::AstralPlane) {
        color.r = static_cast<s16>(
            std::clamp(luma * (astralAccent ? 1.20f : 0.30f), 0.0f, 255.0f));
        color.g = static_cast<s16>(
            std::clamp(luma * (astralAccent ? 0.22f : 0.48f), 0.0f, 255.0f));
        color.b = static_cast<s16>(
            std::clamp(luma * (astralAccent ? 0.30f : 0.95f), 0.0f, 255.0f));
    }
}

}  // namespace

void apply(dScnKy_env_light_c& env) {
    const char* stage = dComIfGp_getStartStageName();
    if (!visual_effects_active() || stage == nullptr || palace_excluded()) return;
    VisualSkybox sky{};
    const u8 variant = static_cast<u8>(runtime_settings().skybox);
    if (!compat::get_authored_sky(sky, variant)) return;

    env.vrbox_sky_col = {sky.sky.r, sky.sky.g, sky.sky.b, env.vrbox_sky_col.a};
    env.vrbox_kumo_top_col = {sky.cloudTop.r, sky.cloudTop.g, sky.cloudTop.b,
                              env.vrbox_kumo_top_col.a};
    env.vrbox_kumo_bottom_col = {sky.cloudBottom.r, sky.cloudBottom.g,
                                 sky.cloudBottom.b, env.vrbox_kumo_bottom_col.a};
    env.vrbox_kumo_shadow_col = {sky.cloudShadow.r, sky.cloudShadow.g,
                                 sky.cloudShadow.b, sky.cloudShadow.a};
    env.vrbox_kasumi_outer_col = {sky.hazeOuter.r, sky.hazeOuter.g,
                                  sky.hazeOuter.b, sky.hazeOuter.a};
    env.vrbox_kasumi_inner_col = {sky.hazeInner.r, sky.hazeInner.g,
                                  sky.hazeInner.b, sky.hazeInner.a};

    // Preserve each authored sky's relative brightness and cloud structure,
    // then color it for the selected visual style. Previously the style pass
    // replaced these values with one fixed palette, making every selection
    // appear identical.
    tint_for_style(env.vrbox_sky_col);
    tint_for_style(env.vrbox_kumo_top_col, true);
    tint_for_style(env.vrbox_kumo_bottom_col);
    tint_for_style(env.vrbox_kumo_shadow_col);
    tint_for_style(env.vrbox_kasumi_outer_col);
    tint_for_style(env.vrbox_kasumi_inner_col, true);
}

}  // namespace twilight_visuals::environment::skybox
