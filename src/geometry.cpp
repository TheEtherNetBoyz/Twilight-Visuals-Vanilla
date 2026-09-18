#include "geometry.hpp"
#include "monochrome.hpp"
#include "runtime.hpp"
#include "boundary.hpp"
#include "hook_api.hpp"
#include "mods/service.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo_rain.h"
#include "d/actor/d_a_bg.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "SSystem/SComponent/c_math.h"
#include <algorithm>
#include <vector>
#include <utility>
#include <cstddef>
#include <cstring>

namespace twilight_visuals::geometry {
namespace {
alignas(32) static u8 s_grassMonochromeKusaTexture[0x800];
alignas(32) static u8 s_grassMonochromeHijikiTexture[0x800];
static TGXTexObj s_grassMonochromeKusaTexObj;
static TGXTexObj s_grassMonochromeHijikiTexObj;
using FogRestore = std::vector<std::pair<J3DFogInfo*, J3DFogInfo>>;
DEFINE_HOOK_SYMBOL("daBg_c::draw", int(daBg_c*), BackgroundDraw);
DEFINE_HOOK(&dKyr_drawSun, CelestialDraw);
DEFINE_HOOK(&dComIfGs_getDate, CelestialDate);
DEFINE_HOOK(&MTXMultVec, CelestialMatrixVector);
FogRestore s_backgroundFogRestore;
bool s_backgroundVisualScope{};
struct CelestialRestore {
    bool active{};
    f32 moonAlpha{};
    f32 sunAlpha{};
    f32 daytime{};
    cXyz moonPosition{};
    GXColorS10 baseLightColor{};
    bool hideVrbox{};
} s_celestialRestore;

HookAction celestial_date_pre(ModContext*, void*, void* retval, void*) {
    if (!s_celestialRestore.active) return HOOK_CONTINUE;
    *static_cast<u16*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

void celestial_matrix_vector_post(ModContext*, void* args, void*, void*) {
    if (!s_celestialRestore.active) return;
    const auto* source = mods::arg<const Vec*>(args, 1);
    auto* destination = mods::arg<Vec*>(args, 2);
    if (source == nullptr || destination == nullptr || std::fabs(source->z) > 0.01f ||
        std::fabs(std::fabs(source->x) - std::fabs(source->y)) > 0.01f) return;
    f32 nativeSize = 8000.0f;
    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr && std::strcmp(stage, "F_SP127") == 0) nativeSize = 11000.0f;
    else if (stage != nullptr && std::strcmp(stage, "F_SP200") == 0) nativeSize = 10000.0f;
    else if (stage != nullptr && std::strcmp(stage, "F_SP103") == 0 && dKy_daynight_check())
        nativeSize = 1200.0f;
    const f32 magnitude = std::fabs(source->x);
    if (std::fabs(magnitude - nativeSize) > 0.5f &&
        std::fabs(magnitude - nativeSize * 2.3f) > 0.5f) return;
    // MFB multiplied dKyr_drawSun's local moon size by four immediately
    // before these camera-billboard transforms. Scaling the resulting offset
    // at the same four transforms is equivalent without patching game code.
    destination->x *= 4.0f;
    destination->y *= 4.0f;
    destination->z *= 4.0f;
}

HookAction celestial_draw_pre(ModContext*, void*, void*, void*) {
    if (!active() || runtime_settings().style != Style::DarkHour || palace_excluded() ||
        g_env_light.mpSunPacket == nullptr) return HOOK_CONTINUE;
    auto* packet = g_env_light.mpSunPacket;
    s_celestialRestore = {true, packet->mMoonAlpha, packet->mSunAlpha,
                          g_env_light.daytime, g_env_light.moon_pos,
                          g_env_light.base_light.mColor, g_env_light.hide_vrbox};
    // MFB's renderer preserved considerably more moon texture detail than vanilla's bloom
    // compositor at full alpha. This pre-bloom alpha reproduces the visible MFB result.
    packet->mMoonAlpha = 0.82f;
    packet->mSunAlpha = 0.0f;
    g_env_light.daytime = 180.0f;
    g_env_light.moon_pos.set(-30000.0f, 45000.0f, -65000.0f);
    // MFB applies MoonPosition after vanilla chooses between its sun-path and
    // moon-path branches. Force the latter branch locally so vanilla computes
    // world = camera eye + the exact MFB relative position.
    g_env_light.base_light.mColor.r = 1;
    g_env_light.hide_vrbox = false;
    return HOOK_CONTINUE;
}

void celestial_draw_post(ModContext*, void*, void*, void*) {
    if (!s_celestialRestore.active || g_env_light.mpSunPacket == nullptr) return;
    g_env_light.mpSunPacket->mMoonAlpha = s_celestialRestore.moonAlpha;
    g_env_light.mpSunPacket->mSunAlpha = s_celestialRestore.sunAlpha;
    g_env_light.daytime = s_celestialRestore.daytime;
    g_env_light.moon_pos = s_celestialRestore.moonPosition;
    g_env_light.base_light.mColor = s_celestialRestore.baseLightColor;
    g_env_light.hide_vrbox = s_celestialRestore.hideVrbox;
    s_celestialRestore.active = false;
}


bool grass_active() {
    bool enabled = false;
    provide_grass(&enabled);
    return enabled;
}
static u8 grass_luminance(u8 r, u8 g, u8 b) {
    return static_cast<u8>((static_cast<u32>(r) * 77 + static_cast<u32>(g) * 150 +
                            static_cast<u32>(b) * 29) >> 8);
}

static void grass_monochrome_color(GXColor& color) {
    const u8 luminance = grass_luminance(color.r, color.g, color.b);
    color.r = luminance;
    color.g = luminance;
    color.b = luminance;
}

static void grass_monochrome_color(GXColorS10& color) {
    const s32 luminance = (static_cast<s32>(color.r) * 77 + static_cast<s32>(color.g) * 150 +
                           static_cast<s32>(color.b) * 29) >> 8;
    const s16 clamped = static_cast<s16>(std::clamp(luminance, -1024, 1023));
    color.r = clamped;
    color.g = clamped;
    color.b = clamped;
}

static void make_grass_monochrome_texture(u8* dst, const u8* src, size_t size) {
    for (size_t i = 0; i < size; i += 2) {
        const u16 pixel = static_cast<u16>((src[i] << 8) | src[i + 1]);
        u16 monochrome;
        if ((pixel & 0x8000) != 0) {
            const u8 r = static_cast<u8>(((pixel >> 10) & 0x1F) * 255 / 31);
            const u8 g = static_cast<u8>(((pixel >> 5) & 0x1F) * 255 / 31);
            const u8 b = static_cast<u8>((pixel & 0x1F) * 255 / 31);
            const u16 gray = static_cast<u16>(grass_luminance(r, g, b) * 31 / 255);
            monochrome = static_cast<u16>(0x8000 | (gray << 10) | (gray << 5) | gray);
        } else {
            const u16 alpha = pixel & 0x7000;
            const u8 r = static_cast<u8>(((pixel >> 8) & 0xF) * 17);
            const u8 g = static_cast<u8>(((pixel >> 4) & 0xF) * 17);
            const u8 b = static_cast<u8>((pixel & 0xF) * 17);
            const u16 gray = static_cast<u16>((grass_luminance(r, g, b) + 8) / 17);
            monochrome = static_cast<u16>(alpha | (gray << 8) | (gray << 4) | gray);
        }
        dst[i] = static_cast<u8>(monochrome >> 8);
        dst[i + 1] = static_cast<u8>(monochrome);
    }
}


void grass_color(void* raw, bool signedChannels) {
    if (!raw || !grass_active()) return;
    if (signedChannels) grass_monochrome_color(*static_cast<GXColorS10*>(raw));
    else grass_monochrome_color(*static_cast<GXColor*>(raw));
}
void* grass_texture(void* original, const u8* pixels, bool first) {
    if (!pixels || !grass_active()) return original;
    static bool ready[2]{};
    const unsigned slot = first ? 0 : 1;
    auto* bytes = first ? s_grassMonochromeKusaTexture : s_grassMonochromeHijikiTexture;
    auto* texture = first ? &s_grassMonochromeKusaTexObj : &s_grassMonochromeHijikiTexObj;
    if (!ready[slot]) {
        make_grass_monochrome_texture(bytes, pixels, 0x800);
        GXInitTexObj(texture, bytes, 32, 32, GX_TF_RGB5A3, GX_REPEAT, GX_CLAMP, GX_FALSE);
        ready[slot] = true;
    }
    return static_cast<GXTexObj*>(texture);
}
bool grass_lighting() {
    if (!grass_active()) return false;
    GXSetChanCtrl(GX_COLOR0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    return true;
}
f32 bloom_gain() {
    return active() ? std::clamp(runtime_settings().brightness, 0.0f, 1.2f) : 1.0f;
}
void after_background(void* viewRaw, void* viewportRaw) {
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || runtime_settings().style != Style::BlackAndWhite ||
        !stage || std::strcmp(stage, "D_MN08") == 0) return;
    auto* view = static_cast<view_class*>(viewRaw);
    auto* viewport = static_cast<view_port_class*>(viewportRaw);
    draw_monochrome_background();
    j3dSys.reinitGX();
    j3dSys.setViewMtx(view->viewMtx);
    GXSetProjection(view->projMtx, GX_PERSPECTIVE);
    GXSetViewport(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height,
                  viewport->near_z, viewport->far_z);
    GXSetScissor(viewport->x_orig, viewport->y_orig, viewport->width, viewport->height);
}
void* before_model(void* modelRaw, void* lightingRaw) {
    if (!active() || runtime_settings().style != Style::AstralPlane ||
        !modelRaw || !lightingRaw) return nullptr;
    auto* model = static_cast<J3DModelData*>(modelRaw);
    auto* lighting = static_cast<dKy_tevstr_c*>(lightingRaw);
    auto* saved = new (std::nothrow) FogRestore;
    if (!saved) return nullptr;
    for (u16 i = 0; i < model->getMaterialNum(); ++i) {
        auto* material = model->getMaterialNodePointer(i);
        if (!material || !material->getFog()) continue;
        auto* fog = material->getFog()->getFogInfo();
        if (!fog || fog->mType != 0) continue;
        saved->emplace_back(fog, *fog);
        fog->mType = 2;
        fog->mStartZ = lighting->mFogStartZ;
        fog->mEndZ = lighting->mFogEndZ;
        fog->mColor = {static_cast<u8>(lighting->FogCol.r), static_cast<u8>(lighting->FogCol.g),
                       static_cast<u8>(lighting->FogCol.b), 255};
        if (auto* view = dComIfGd_getView()) {
            fog->mNearZ = view->near_;
            fog->mFarZ = view->far_;
        }
    }
    return saved;
}
void after_model(void* token) {
    auto* saved = static_cast<FogRestore*>(token);
    if (!saved) return;
    for (auto& entry : *saved) *entry.first = entry.second;
    delete saved;
}

HookAction background_draw_pre(ModContext*, void* args, void*, void*) {
    s_backgroundVisualScope = active() && !palace_excluded();
    if (s_backgroundVisualScope) boundary::begin_visual_environment();
    auto* background = mods::arg<daBg_c*>(args, 0);
    if (!background || !active() || runtime_settings().style != Style::AstralPlane)
        return HOOK_CONTINUE;

    // MFB applied before_model immediately before each stage-background draw.
    // A vanilla draw hook can prepare all six background parts up front and
    // restore them after daBg_c::draw returns, covering floors and terrain.
    s_backgroundFogRestore.clear();
    for (auto& part : background->mBgParts) {
        if (!part.model || !part.tevstr) continue;
        auto* data = part.model->getModelData();
        for (u16 i = 0; i < data->getMaterialNum(); ++i) {
            auto* material = data->getMaterialNodePointer(i);
            if (!material || !material->getFog()) continue;
            auto* fog = material->getFog()->getFogInfo();
            if (!fog || fog->mType != 0) continue;
            s_backgroundFogRestore.emplace_back(fog, *fog);
            fog->mType = 2;
            fog->mStartZ = part.tevstr->mFogStartZ;
            fog->mEndZ = part.tevstr->mFogEndZ;
            fog->mColor = {static_cast<u8>(part.tevstr->FogCol.r),
                           static_cast<u8>(part.tevstr->FogCol.g),
                           static_cast<u8>(part.tevstr->FogCol.b), 255};
            if (auto* view = dComIfGd_getView()) {
                fog->mNearZ = view->near_;
                fog->mFarZ = view->far_;
            }
        }
    }
    return HOOK_CONTINUE;
}

void background_draw_post(ModContext*, void*, void*, void*) {
    for (auto& entry : s_backgroundFogRestore) *entry.first = entry.second;
    s_backgroundFogRestore.clear();
    if (s_backgroundVisualScope) boundary::end_visual_environment();
    s_backgroundVisualScope = false;
}
void particle(cXyz* corners, cXyz* position, void* rawColor, u32 j, f32 presentationCounter) {
    if (!active() || runtime_settings().style != Style::AstralPlane) return;
    auto& color = *static_cast<GXColor*>(rawColor);
    const u32 seed = (j + 1u) * 2654435761u;
    const f32 width = 0.3f + (seed & 255u) / 255.0f;
    const f32 height = 0.4f + ((seed >> 8) & 255u) / 100.0f;
    for (int k = 0; k < 4; ++k) {
        corners[k].x *= width;
        corners[k].y *= height;
    }
    if (j % 3 == 0) corners[3] = corners[2];
    else corners[1].x *= 0.2f;
    const f32 phase = (seed & 65535u) * (6.2831853f / 65536.0f);
    const s16 angle = static_cast<s16>(32767.0f * cM_fsin(
        phase + presentationCounter * (0.0045f + (j % 89) * 0.0001f)));
    const f32 sine = cM_ssin(angle), cosine = cM_scos(angle);
    for (int k = 0; k < 4; ++k) {
        const f32 x = corners[k].x;
        corners[k].x = x * cosine - corners[k].y * sine;
        corners[k].y = x * sine + corners[k].y * cosine;
    }
    position->x += 22.0f * cM_fsin(phase + presentationCounter * (0.0037f + (j % 61) * 0.0001f));
    position->z += 18.0f * cM_fcos(phase + presentationCounter * (0.0029f + (j % 47) * 0.0001f));
    color.r = j % 7 == 0 ? 130 : 10;
    color.g = j % 7 == 0 ? 38 : 17;
    color.b = j % 7 == 0 ? 52 : 29;
}
}
void transform_particle(cXyz* corners, cXyz* position, void* color, u32 index, f32 time) {
    particle(corners, position, color, index, time);
}
bool initialize() {
    const ModResult pre = mods::hook::add_pre<BackgroundDraw>(background_draw_pre);
    if (pre != MOD_OK) return false;
    const ModResult post = mods::hook::add_post<BackgroundDraw>(background_draw_post);
    if (post != MOD_OK) {
        mods::hook::uninstall<BackgroundDraw>();
        return false;
    }
    const ModResult celestialPre = mods::hook::add_pre<CelestialDraw>(celestial_draw_pre);
    if (celestialPre != MOD_OK) return false;
    const ModResult celestialPost = mods::hook::add_post<CelestialDraw>(celestial_draw_post);
    if (celestialPost != MOD_OK) {
        mods::hook::uninstall<CelestialDraw>();
        return false;
    }
    if (mods::hook::add_pre<CelestialDate>(celestial_date_pre) != MOD_OK) {
        mods::hook::uninstall<CelestialDraw>();
        return false;
    }
    if (mods::hook::add_post<CelestialMatrixVector>(celestial_matrix_vector_post) != MOD_OK) {
        mods::hook::uninstall<CelestialDate>();
        mods::hook::uninstall<CelestialDraw>();
        return false;
    }
    return true;
}
void shutdown() {
    celestial_draw_post(nullptr, nullptr, nullptr, nullptr);
    mods::hook::uninstall<CelestialMatrixVector>();
    mods::hook::uninstall<CelestialDate>();
    mods::hook::uninstall<CelestialDraw>();
    background_draw_post(nullptr, nullptr, nullptr, nullptr);
    mods::hook::uninstall<BackgroundDraw>();
}
}
