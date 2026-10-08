#include "environment_palette.hpp"

#include "environment.hpp"
#include "environment_fog.hpp"
#include "runtime.hpp"
#include "boundary.hpp"

#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace twilight_visuals::environment::palette {

namespace {
bool environment_active() {
    const char* stage = dComIfGp_getStartStageName();
    return visual_effects_active() && stage != nullptr && !palace_excluded();
}

// Dusklight's global visual-Twilight option only converts rooms that are not
// already in native Twilight. Native Twilight rooms already carry their
// authored sky, fog, lights, and bloom and must never be processed twice.
bool emulated_normal_twilight() {
    // Every non-native room uses one deterministic visual conversion. Layer 14
    // is not a portable indication of an authored Twilight environment and was
    // responsible for both green skies and cleared Twilight regions receiving
    // no conversion at all.
    return environment_active() && runtime_settings().style == Style::Normal &&
           dComIfG_play_c::getLayerNo(0) != 14 &&
           !boundary::using_authored_twilight_environment();
}

bool dark_hour_dungeon_indoor() {
    if (!dark_hour_indoor()) return false;
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr &&
           (std::strncmp(stage, "D_MN", 4) == 0 ||
            std::strncmp(stage, "D_SB", 4) == 0);
}

bool palace_dark_hour() {
    const char* stage = dComIfGp_getStartStageName();
    return runtime_settings().style == Style::DarkHour && stage != nullptr &&
           std::strncmp(stage, "D_MN08", 6) == 0;
}

s16 scale_channel(s16 value, float factor) {
    return static_cast<s16>(std::clamp(value * factor, -1024.0f, 1023.0f));
}

u8 scale_channel(u8 value, float factor) {
    return static_cast<u8>(std::clamp(value * factor, 0.0f, 255.0f));
}

void scale_color(GXColorS10& color, float factor) {
    color.r = scale_channel(color.r, factor);
    color.g = scale_channel(color.g, factor);
    color.b = scale_channel(color.b, factor);
}

void scale_light(J3DLightObj& light, float factor) {
    J3DLightInfo* info = light.getLightInfo();
    info->mColor.r = scale_channel(info->mColor.r, factor);
    info->mColor.g = scale_channel(info->mColor.g, factor);
    info->mColor.b = scale_channel(info->mColor.b, factor);
}

void normal_twilight_tint(GXColorS10& color, u8 red, u8 green, u8 blue) {
    color.r = static_cast<s16>(std::clamp((static_cast<s32>(color.r) * red) / 255, 0, 255));
    color.g = static_cast<s16>(std::clamp((static_cast<s32>(color.g) * green) / 255, 0, 255));
    color.b = static_cast<s16>(std::clamp((static_cast<s32>(color.b) * blue) / 255, 0, 255));
}

void normal_twilight_tint(J3DLightObj& light, u8 red, u8 green, u8 blue) {
    J3DLightInfo* info = light.getLightInfo();
    info->mColor.r = static_cast<u8>((static_cast<u32>(info->mColor.r) * red) / 255);
    info->mColor.g = static_cast<u8>((static_cast<u32>(info->mColor.g) * green) / 255);
    info->mColor.b = static_cast<u8>((static_cast<u32>(info->mColor.b) * blue) / 255);
}

void apply_normal_twilight_environment(dScnKy_env_light_c& env) {
    if (!environment_active() || runtime_settings().style != Style::Normal ||
        dComIfG_play_c::getLayerNo(0) == 14) return;

    if (emulated_normal_twilight()) {
        normal_twilight_tint(env.actor_amb_col, 205, 180, 218);
        for (int i = 0; i < 4; ++i)
            normal_twilight_tint(env.bg_amb_col[i], 176, 169, 208);
        for (int i = 0; i < 6; ++i)
            normal_twilight_tint(env.dungeonlight_col[i], 214, 193, 224);
        fog::apply_normal_twilight(env.fog_col, env.mFogNear, env.mFogFar);
    }

    // Some rooms provide authored Twilight lighting tables but no VRB table.
    // Preserve their authored lighting while supplying the missing canonical
    // Twilight sky instead of leaving the room under its normal gray sky.
    if (boundary::using_authored_twilight_sky()) return;
    env.vrbox_sky_col = {24, 48, 72, 255};
    env.vrbox_kumo_top_col = {112, 108, 80, 176};
    env.vrbox_kumo_bottom_col = {43, 68, 79, 255};
    env.vrbox_kumo_shadow_col = {18, 29, 45, 176};
    env.vrbox_kasumi_outer_col = {119, 91, 49, 255};
    env.vrbox_kasumi_inner_col = {57, 75, 81, 255};
    env.hide_vrbox = false;

}

// Dark Hour should keep the authored differences between areas, but bright
// Outdoor palettes must not turn into clipped neon when the green tint is
// applied. Indoor rooms use this same clamp before switching to blue.
float dark_hour_environment_exposure(float luma) {
    constexpr float referenceLuma = 300.0f;
    constexpr float minimumExposure = 0.36f;
    const float outdoorExposure = luma <= referenceLuma
        ? 1.0f
        : std::clamp(referenceLuma / luma, minimumExposure, 1.0f);
    // The Forest Temple bridge uses the outdoor sky, but its pale materials
    // start much brighter than ordinary field terrain.
    if (reduced_dark_hour_outdoor()) return outdoorExposure * reduced_dark_hour_outdoor_scale();
    if (dark_hour_dungeon_indoor()) return outdoorExposure * 0.42f;
    return dark_hour_indoor() ? outdoorExposure * 0.68f : outdoorExposure;
}

// Bloom only processes the bright part of the finished frame. Keep the bloom
// preset shared with Twilight, but shift the background TEV input toward the
// Dark Hour green before the terrain is rendered. This is intentionally
// background-only: Link and other actors keep their authored material colors.
void tint_dark_hour_background_color(GXColorS10& color) {
    if (runtime_settings().style != Style::DarkHour) return;
    const float luma = std::max(0.0f, color.r * 0.25f + color.g * 0.65f + color.b * 0.10f);
    const float exposure = dark_hour_environment_exposure(luma);
    if (dark_hour_indoor()) {
        color.r = static_cast<s16>(std::clamp(14.0f + luma * 0.42f * exposure, 0.0f, 1023.0f));
        color.g = static_cast<s16>(std::clamp(22.0f + luma * 0.60f * exposure, 0.0f, 1023.0f));
        color.b = static_cast<s16>(std::clamp(36.0f + luma * 0.86f * exposure, 0.0f, 1023.0f));
        return;
    }
    // Gerudo's sand becomes neon green under the shared Dark Hour grade.
    // Correct only the stage's background/terrain colors; actor lighting and
    // every other Dark Hour area keep their existing grade.
    const float desertScale = gerudo_desert() ? 0.82f : 1.0f;
    const float desertGreenScale = gerudo_desert() ? 0.62f : 1.0f;
    color.r = scale_channel(color.r, 0.42f * exposure * desertScale);
    color.g = scale_channel(color.g, 1.10f * exposure * desertScale * desertGreenScale);
    color.b = scale_channel(color.b, 0.50f * exposure * desertScale);
}

void tint_dark_hour_background_light(J3DLightObj& light) {
    if (runtime_settings().style != Style::DarkHour) return;
    J3DLightInfo* info = light.getLightInfo();
    const float luma = std::max(0.0f, info->mColor.r * 0.25f + info->mColor.g * 0.65f +
                                           info->mColor.b * 0.10f);
    const float exposure = dark_hour_environment_exposure(luma);
    if (dark_hour_indoor()) {
        info->mColor.r = static_cast<u8>(std::clamp(10.0f + luma * 0.42f * exposure, 0.0f, 255.0f));
        info->mColor.g = static_cast<u8>(std::clamp(16.0f + luma * 0.60f * exposure, 0.0f, 255.0f));
        info->mColor.b = static_cast<u8>(std::clamp(26.0f + luma * 0.86f * exposure, 0.0f, 255.0f));
        return;
    }
    const float desertScale = gerudo_desert() ? 0.82f : 1.0f;
    const float desertGreenScale = gerudo_desert() ? 0.62f : 1.0f;
    info->mColor.r = scale_channel(info->mColor.r, 0.42f * exposure * desertScale);
    info->mColor.g = scale_channel(info->mColor.g, 1.10f * exposure * desertScale * desertGreenScale);
    info->mColor.b = scale_channel(info->mColor.b, 0.50f * exposure * desertScale);
}

void apply_indoor_window_accent(dKy_tevstr_c& tev) {
    if (runtime_settings().style != Style::DarkHour || !dark_hour_indoor()) return;

    // Reuse one authored room light as the exterior spill. Keeping its native
    // position, direction and attenuation makes the accent land on geometry
    // naturally instead of washing the entire room teal.
    int strongest = -1;
    float strongestLuma = 0.0f;
    for (int i = 0; i < 6; ++i) {
        const GXColor& color = tev.mLights[i].getLightInfo()->mColor;
        const float luma = color.r * 0.25f + color.g * 0.65f + color.b * 0.10f;
        if (luma > strongestLuma) {
            strongestLuma = luma;
            strongest = i;
        }
    }
    if (strongest < 0 || strongestLuma < 3.0f) return;

    GXColor& color = tev.mLights[strongest].getLightInfo()->mColor;
    const float accentScale = dark_hour_dungeon_indoor() ? 0.44f : 0.72f;
    const float accentMaximum = dark_hour_dungeon_indoor() ? 70.0f : 112.0f;
    const float energy = std::clamp(strongestLuma * accentScale, 14.0f, accentMaximum);
    color.r = static_cast<u8>(std::clamp(energy * 0.28f, 0.0f, 255.0f));
    color.g = static_cast<u8>(std::clamp(energy * 1.00f, 0.0f, 255.0f));
    color.b = static_cast<u8>(std::clamp(energy * 0.48f, 0.0f, 255.0f));
}

void apply_palace_indoor_actor_fill(dKy_tevstr_c& tev) {
    if (!palace_dark_hour() || !dark_hour_indoor()) return;

    // Palace authors several actor environments with an almost black ambient.
    // The shared indoor conversion correctly colors that input but cannot create
    // visibility from zero. Supply a restrained cool fill only to the actor TEV;
    // directional room lights still provide the shape and exterior-green accents.
    tev.AmbCol.r = std::max<s16>(tev.AmbCol.r, 52);
    tev.AmbCol.g = std::max<s16>(tev.AmbCol.g, 70);
    tev.AmbCol.b = std::max<s16>(tev.AmbCol.b, 100);
}

void apply_outdoor_moonlight(dKy_tevstr_c& tev) {
    if (runtime_settings().style != Style::DarkHour || dark_hour_indoor()) return;

    // Preserve the stage-authored direction and attenuation, but turn its
    // strongest light into a restrained moon key. This gives actors and drops
    // a readable green-white highlight without lifting the ambient exposure.
    int strongest = -1;
    float strongestLuma = 0.0f;
    for (int i = 0; i < 6; ++i) {
        const GXColor& color = tev.mLights[i].getLightInfo()->mColor;
        const float luma = color.r * 0.25f + color.g * 0.65f + color.b * 0.10f;
        if (luma > strongestLuma) {
            strongestLuma = luma;
            strongest = i;
        }
    }
    if (strongest >= 0) {
        GXColor& color = tev.mLights[strongest].getLightInfo()->mColor;
        const float minimumEnergy = gerudo_desert() ? 40.0f : 58.0f;
        const float energy = std::clamp(strongestLuma, minimumEnergy, 108.0f);
        color.r = static_cast<u8>(std::clamp(energy * 0.58f, 0.0f, 255.0f));
        color.g = static_cast<u8>(std::clamp(energy, 0.0f, 255.0f));
        color.b = static_cast<u8>(std::clamp(energy * 0.76f, 0.0f, 255.0f));
    }

    // Vanilla projected shadows (including simple item-drop shadows) multiply
    // their opacity by this per-object environment value.
    tev.field_0x344 = std::clamp(std::max(tev.field_0x344, 0.72f), 0.0f, 1.0f);
}

void grayscale(GXColorS10& color) {
    const s32 luma = (static_cast<s32>(color.r) * 77 + static_cast<s32>(color.g) * 150 +
                         static_cast<s32>(color.b) * 29) >>
                     8;
    color.r = color.g = color.b = static_cast<s16>(std::clamp(luma, -1024, 1023));
}

void grayscale(GXColor& color) {
    const u8 luma = static_cast<u8>((static_cast<u32>(color.r) * 77 +
                                       static_cast<u32>(color.g) * 150 +
                                       static_cast<u32>(color.b) * 29) >>
                                   8);
    color.r = color.g = color.b = luma;
}

void grayscale(J3DLightObj& light) {
    J3DLightInfo* info = light.getLightInfo();
    const u8 luma = static_cast<u8>((static_cast<u32>(info->mColor.r) * 77 +
                                       static_cast<u32>(info->mColor.g) * 150 +
                                       static_cast<u32>(info->mColor.b) * 29) >>
                                   8);
    info->mColor.r = info->mColor.g = info->mColor.b = luma;
}

float brightness() {
    if (!environment_active()) return 1.0f;
    // Normal Twilight is a fidelity preset. Its layer-14 palette already has
    // the complete authored exposure. Kakariko's visual-only table runs a bit
    // hotter than the same table under the native Twilight state, so apply a
    // restrained stage correction without changing other regions.
    if (runtime_settings().style == Style::Normal)
        return kakariko_village() ? 0.92f : 1.0f;
    float value = runtime_settings().brightness;
    if (runtime_settings().style == Style::AstralPlane) value *= 0.65f;
    return std::clamp(value, 0.0f, 1.2f);
}

void tint_astral_light(J3DLightObj& light, bool redAccent) {
    if (!environment_active() || runtime_settings().style != Style::AstralPlane) return;
    J3DLightInfo* info = light.getLightInfo();
    const float luma =
        (info->mColor.r * 0.299f + info->mColor.g * 0.587f + info->mColor.b * 0.114f) * 0.55f;
    info->mColor.r = static_cast<u8>(
        std::clamp(luma * (redAccent ? 1.30f : 0.30f), 0.0f, 255.0f));
    info->mColor.g = static_cast<u8>(
        std::clamp(luma * (redAccent ? 0.20f : 0.48f), 0.0f, 255.0f));
    info->mColor.b = static_cast<u8>(
        std::clamp(luma * (redAccent ? 0.28f : 1.05f), 0.0f, 255.0f));
}

void apply_astral_palette(dScnKy_env_light_c& env) {
    if (!environment_active() || runtime_settings().style != Style::AstralPlane) return;
    const auto tint = [](GXColorS10& color, bool warm) {
        const float luma = 0.60f * std::max(
                                      0.0f, color.r * 0.299f + color.g * 0.587f + color.b * 0.114f);
        color.r = static_cast<s16>(
            std::clamp(luma * (warm ? 1.20f : 0.30f), 0.0f, 1023.0f));
        color.g = static_cast<s16>(
            std::clamp(luma * (warm ? 0.22f : 0.48f), 0.0f, 1023.0f));
        color.b = static_cast<s16>(
            std::clamp(luma * (warm ? 0.30f : 0.95f), 0.0f, 1023.0f));
    };
    for (int i = 0; i < 4; ++i) tint(env.bg_amb_col[i], i == 1);
    for (int i = 0; i < 6; ++i) tint(env.dungeonlight_col[i], true);
    tint(env.actor_amb_col, false);
    env.fog_col = {49, 73, 91, env.fog_col.a};
    env.vrbox_sky_col = {24, 43, 62, env.vrbox_sky_col.a};
    env.vrbox_kumo_top_col = {110, 58, 72, env.vrbox_kumo_top_col.a};
    env.vrbox_kumo_bottom_col = {31, 48, 68, env.vrbox_kumo_bottom_col.a};
    env.vrbox_kumo_shadow_col = {17, 28, 45, env.vrbox_kumo_shadow_col.a};
    env.vrbox_kasumi_outer_col = {48, 70, 88, env.vrbox_kasumi_outer_col.a};
    env.vrbox_kasumi_inner_col = {119, 72, 85, env.vrbox_kasumi_inner_col.a};
    scale_color(env.fog_col, 0.75f);
    scale_color(env.vrbox_sky_col, 0.75f);
    scale_color(env.vrbox_kumo_top_col, 0.75f);
    scale_color(env.vrbox_kumo_bottom_col, 0.75f);
    scale_color(env.vrbox_kumo_shadow_col, 0.75f);
    scale_color(env.vrbox_kasumi_outer_col, 0.75f);
    scale_color(env.vrbox_kasumi_inner_col, 0.75f);
}

void apply_dark_hour_palette(dScnKy_env_light_c& env) {
    // Palace keeps its native dungeon/floor lighting so the corrected ground
    // response is not disturbed, but its sky must use the same Dark Hour
    // atmosphere as every other outdoor scene. Apply the shared sky palette
    // below for both paths and keep only the material-light changes non-Palace.
    if (!environment_active() || runtime_settings().style != Style::DarkHour) return;
    const auto tint = [](GXColorS10& color) {
        const float luma = std::max(0.0f, color.r * 0.25f + color.g * 0.65f + color.b * 0.10f);
        const float exposure = dark_hour_environment_exposure(luma);
        if (dark_hour_indoor()) {
            color.r = static_cast<s16>(std::clamp(14.0f + luma * 0.42f * exposure, 0.0f, 1023.0f));
            color.g = static_cast<s16>(std::clamp(22.0f + luma * 0.60f * exposure, 0.0f, 1023.0f));
            color.b = static_cast<s16>(std::clamp(36.0f + luma * 0.86f * exposure, 0.0f, 1023.0f));
        } else {
            color.r = static_cast<s16>(std::clamp(luma * 0.28f * exposure, 0.0f, 1023.0f));
            color.g = static_cast<s16>(std::clamp(luma * 1.08f * exposure, 0.0f, 1023.0f));
            color.b = static_cast<s16>(std::clamp(luma * 0.40f * exposure, 0.0f, 1023.0f));
        }
    };
    // Palace uses the same Dark Hour high-end clamp as the overworld. Leaving
    // its authored ambient/dungeon values untouched is what caused bright
    // Palace rooms to blow out while other areas stayed regulated.
    for (int i = 0; i < 4; ++i) tint(env.bg_amb_col[i]);
    for (int i = 0; i < 6; ++i) tint(env.dungeonlight_col[i]);
    if (dark_hour_indoor()) tint(env.actor_amb_col);
    // MFB supplied these colors before the host's visual-Twilight sky-volume response. Vanilla
    // has no separate visual query, so bake that response into the final cloud and haze colors.
    env.vrbox_sky_col = {15, 66, 29, env.vrbox_sky_col.a};
    env.vrbox_kumo_top_col = {44, 137, 60, env.vrbox_kumo_top_col.a};
    env.vrbox_kumo_bottom_col = {20, 84, 38, env.vrbox_kumo_bottom_col.a};
    env.vrbox_kumo_shadow_col = {7, 38, 18, env.vrbox_kumo_shadow_col.a};
    env.vrbox_kasumi_outer_col = {24, 91, 42, env.vrbox_kasumi_outer_col.a};
    env.vrbox_kasumi_inner_col = {54, 148, 70, env.vrbox_kasumi_inner_col.a};
}

}  // namespace

bool active() {
    return environment_active();
}

void apply_scene_base(dScnKy_env_light_c& env) {
    apply_normal_twilight_environment(env);
    apply_astral_palette(env);
    apply_dark_hour_palette(env);
}

void finish_scene(dScnKy_env_light_c& env) {
    const float factor = brightness();
    scale_color(env.actor_amb_col, factor);
    for (int i = 0; i < 4; ++i) scale_color(env.bg_amb_col[i], factor);
    for (int i = 0; i < 6; ++i) {
        scale_color(env.dungeonlight_col[i], factor);
        env.dungeonlight[i].mColor.r =
            static_cast<u8>(std::clamp<s16>(env.dungeonlight_col[i].r, 0, 255));
        env.dungeonlight[i].mColor.g =
            static_cast<u8>(std::clamp<s16>(env.dungeonlight_col[i].g, 0, 255));
        env.dungeonlight[i].mColor.b =
            static_cast<u8>(std::clamp<s16>(env.dungeonlight_col[i].b, 0, 255));
    }
    scale_color(env.fog_col, factor);
    scale_color(env.vrbox_sky_col, factor);
    scale_color(env.vrbox_kumo_top_col, factor);
    scale_color(env.vrbox_kumo_bottom_col, factor);
    scale_color(env.vrbox_kumo_shadow_col, factor);
    scale_color(env.vrbox_kasumi_outer_col, factor);
    scale_color(env.vrbox_kasumi_inner_col, factor);

    if (runtime_settings().style == Style::BlackAndWhite) {
        for (int i = 0; i < 4; ++i) grayscale(env.bg_amb_col[i]);
        grayscale(env.fog_col);
        grayscale(env.vrbox_sky_col);
        grayscale(env.vrbox_kumo_top_col);
        grayscale(env.vrbox_kumo_bottom_col);
        grayscale(env.vrbox_kumo_shadow_col);
        grayscale(env.vrbox_kasumi_outer_col);
        grayscale(env.vrbox_kasumi_inner_col);
    }

    GXColor blend = *mDoGph_gInf_c::getBloom()->getBlendColor();
    GXColor mono = *mDoGph_gInf_c::getBloom()->getMonoColor();
    if (runtime_settings().style == Style::AstralPlane) {
        blend.r = 180;
        blend.g = 40;
        blend.b = 130;
        mono.a = 0;
    } else if (runtime_settings().style == Style::DarkHour) {
        if (dark_hour_indoor()) {
            blend.r = 30;
            blend.g = 210;
            blend.b = 66;
        } else {
            blend.r = 24;
            blend.g = 220;
            blend.b = 52;
        }
        mono.a = 0;
    } else if (runtime_settings().style == Style::BlackAndWhite) {
        grayscale(blend);
        grayscale(mono);
    }
    mDoGph_gInf_c::getBloom()->setBlendColor(blend);
    mDoGph_gInf_c::getBloom()->setMonoColor(mono);
}

void apply_background(dKy_tevstr_c& tev, GXColorS10* colors, GXColorS10& fogColor) {
    if (emulated_normal_twilight()) {
        for (int i = 0; i < 4; ++i) normal_twilight_tint(colors[i], 176, 169, 208);
        for (int i = 0; i < 6; ++i) normal_twilight_tint(tev.mLights[i], 214, 193, 224);
    }
    const float factor = brightness();
    for (int i = 0; i < 4; ++i) {
        tint_dark_hour_background_color(colors[i]);
        scale_color(colors[i], factor);
    }
    for (int i = 0; i < 6; ++i) {
        tint_dark_hour_background_light(tev.mLights[i]);
        scale_light(tev.mLights[i], factor);
        tint_astral_light(tev.mLights[i], i == 1 || i == 4);
    }
    apply_indoor_window_accent(tev);
    apply_outdoor_moonlight(tev);
    scale_color(fogColor, factor);
    if (runtime_settings().style == Style::BlackAndWhite) {
        for (int i = 0; i < 4; ++i) grayscale(colors[i]);
        for (int i = 0; i < 6; ++i) grayscale(tev.mLights[i]);
        grayscale(fogColor);
    }
}

void apply_actor(dKy_tevstr_c& tev, GXColorS10& fogColor) {
    if (emulated_normal_twilight()) {
        normal_twilight_tint(tev.AmbCol, 205, 180, 218);
        for (int i = 0; i < 6; ++i) normal_twilight_tint(tev.mLights[i], 214, 193, 224);
    }
    if (dark_hour_indoor()) tint_dark_hour_background_color(tev.AmbCol);
    apply_palace_indoor_actor_fill(tev);
    const float factor = brightness();
    scale_color(tev.AmbCol, factor);
    for (int i = 0; i < 6; ++i) {
        if (dark_hour_indoor()) tint_dark_hour_background_light(tev.mLights[i]);
        scale_light(tev.mLights[i], factor);
        tint_astral_light(tev.mLights[i], i == 1 || i == 4);
    }
    apply_indoor_window_accent(tev);
    apply_outdoor_moonlight(tev);
    scale_color(fogColor, factor);
}

}  // namespace twilight_visuals::environment::palette

