#include "particles.hpp"
#include "native_particles.hpp"
#include "blood.hpp"
#include "run_trail.hpp"
#include "compat.hpp"
#include "runtime.hpp"
#include "boundary.hpp"
#include "mods/service.hpp"
#include "hook_api.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_rain.h"
#include "d/d_kankyo_wether.h"
#include "d/d_stage.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_graphic.h"
#include <algorithm>
#include <cstring>

namespace twilight_visuals::particles {
namespace {
DEFINE_HOOK(&dKyw_wether_move_draw, WeatherMoveDraw);
DEFINE_HOOK(&dKyw_wether_move_draw2, WeatherMoveDraw2);
DEFINE_HOOK(&dKyw_wether_draw, WeatherDraw);
DEFINE_HOOK(&dKyw_wether_proc, WeatherProc);
DEFINE_HOOK(&dKyr_drawRain, RainDraw);

void draw_blood_rain() {
    dKankyo_rain_Packet* packet = g_env_light.mpRainPacket;
    camera_class* camera = static_cast<camera_class*>(dComIfGp_getCamera(0));
    if (packet == nullptr || packet->mpTex == nullptr || camera == nullptr ||
        packet->raincnt <= 0 || g_env_light.camera_water_in_status != 0 ||
        dComIfGd_getView() == nullptr)
        return;

    static TGXTexObj rainTexture;
    static ResTIMG* loadedImage = nullptr;
    ResTIMG* image = reinterpret_cast<ResTIMG*>(packet->mpTex);
    if (loadedImage != image) {
        if (loadedImage != nullptr) rainTexture.reset();
        loadedImage = image;
        GXInitTexObj(&rainTexture, (&image->format + image->imageOffset),
                     image->width, image->height, static_cast<GXTexFmt>(image->format),
                     static_cast<GXTexWrapMode>(image->wrapS),
                     static_cast<GXTexWrapMode>(image->wrapT),
                     static_cast<GXBool>(image->mipmapCount > 1));
        GXInitTexObjLOD(&rainTexture, static_cast<GXTexFilter>(image->minFilter),
                        static_cast<GXTexFilter>(image->magFilter),
                        image->minLOD * 0.125f, image->maxLOD * 0.125f,
                        image->LODBias * 0.01f, static_cast<GXBool>(image->biasClamp),
                        static_cast<GXBool>(image->doEdgeLOD),
                        static_cast<GXAnisotropy>(image->maxAnisotropy));
    }
    GXLoadTexObj(&rainTexture, GX_TEXMAP0);

    Mtx cameraBillboard;
    MTXInverse(dComIfGd_getView()->viewMtxNoTrans, cameraBillboard);
    GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_C0, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                    GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_A0, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                    GX_TRUE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_GREATER, 0);
    GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_DISABLE);
    GXSetClipMode(GX_CLIP_DISABLE);
    GXSetNumIndStages(0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_RGBA4, 8);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    const int count = std::min<int>(packet->raincnt, 250);
    const cXyz wind = dKyw_get_wind_vecpow();
    const cXyz fall(0.0f, -2.0f, 0.0f);
    Vec localSide;
    Vec side;
    cXyz position[4];
    for (int i = 0; i < count; ++i) {
        const RAIN_EFF& effect = packet->mRainEff[i];
        if (effect.mAlpha <= 0.0f) continue;

        cXyz origin = effect.mBasePos + effect.mPosition;
        const f32 distance = std::min(1.0f, 0.1f + origin.abs(camera->view.lookat.eye) / 1500.0f);
        const f32 speed = 5.0f + 70.0f * distance;
        cXyz tilt;
        tilt.x = speed * (fall.x + 0.08f * (i & 7) +
                          10.0f * packet->mCenterDelta.x * packet->mCenterDeltaMul + wind.x);
        tilt.y = speed * (fall.y + packet->mCenterDelta.y * packet->mCenterDeltaMul + wind.y);
        tilt.z = speed * (fall.z + 0.08f * (i & 3) +
                          10.0f * packet->mCenterDelta.z * packet->mCenterDeltaMul + wind.z);

        localSide = {-1.5f, 0.0f, 0.0f};
        cMtx_multVec(cameraBillboard, &localSide, &side);
        position[0].set(origin.x + side.x - tilt.x, origin.y + side.y - tilt.y,
                        origin.z + side.z - tilt.z);
        localSide.x = 1.5f;
        cMtx_multVec(cameraBillboard, &localSide, &side);
        position[1].set(origin.x + side.x - tilt.x, origin.y + side.y - tilt.y,
                        origin.z + side.z - tilt.z);
        position[2].set(origin.x + side.x, origin.y + side.y, origin.z + side.z);
        localSide.x = -1.5f;
        cMtx_multVec(cameraBillboard, &localSide, &side);
        position[3].set(origin.x + side.x, origin.y + side.y, origin.z + side.z);

        GXColor color{190, 4, 12,
                      static_cast<u8>(std::clamp(effect.mAlpha * 42.0f, 0.0f, 118.0f))};
        GXSetTevColor(GX_TEVREG0, color);

        static const cXyz offsets[] = {
            cXyz(150.0f, 0.0f, 0.0f), cXyz(0.0f, 150.0f, 150.0f),
            cXyz(150.0f, 320.0f, 150.0f), cXyz(45.0f, 480.0f, 45.0f),
        };
        for (const cXyz& offset : offsets) {
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(position[0].x + offset.x, position[0].y + offset.y, position[0].z + offset.z); GXTexCoord2s16(0, 0);
            GXPosition3f32(position[1].x + offset.x, position[1].y + offset.y, position[1].z + offset.z); GXTexCoord2s16(0xFF, 0);
            GXPosition3f32(position[2].x + offset.x, position[2].y + offset.y, position[2].z + offset.z); GXTexCoord2s16(0xFF, 0xFF);
            GXPosition3f32(position[3].x + offset.x, position[3].y + offset.y, position[3].z + offset.z); GXTexCoord2s16(0, 0xFF);
            GXEnd();
        }
    }
    GXSetClipMode(GX_CLIP_ENABLE);
    J3DShape::resetVcdVatCache();
}

HookAction rain_draw_pre(ModContext*, void*, void*, void*) {
    if (!active() || runtime_settings().weather != Weather::BloodRain) return HOOK_CONTINUE;
    draw_blood_rain();
    return HOOK_SKIP_ORIGINAL;
}

class VisualHousiPacket final : public dKankyo_housi_Packet {
public:
    void draw() override {
        Mtx drawMtx;
        MTXCopy(j3dSys.getViewMtx(), drawMtx);
        if (g_env_light.camera_water_in_status != 0 && dComIfGd_getView() != nullptr)
            MTXCopy(dComIfGd_getView()->viewMtx, drawMtx);
        const f32 timeScale = runtime_settings().style == Style::AstralPlane ? 0.5f : 1.0f;
        native_particles::draw(drawMtx, &mpResTex, this, timeScale);
    }
};

VisualHousiPacket* g_packet{};
bool g_nativeHousiDrawSuppressed{};
u8 g_savedHousiInitialized{};
bool g_nativeStarDrawSuppressed{};
u8 g_savedStarInitialized{};
bool g_savedNativeState{};
bool g_savedDarkHourVrbox{};
bool g_darkHourVrboxTemporarilyShown{};
bool g_moveVisualScope{};
bool g_drawVisualScope{};
bool g_cloudVisualScope{};
bool g_weatherProcVisualScope{};
int g_nativeCount{};
u8 g_nativeType{};

bool native_visual_context() {
    if (!active() || palace_excluded()) return false;
    // MFB forces the shared sun/moon packet to exist before reporting visual
    // Twilight to the weather code. Vanilla has no ForceMoon callback, so let
    // it create that packet normally on the first weather tick, then expose
    // visual Twilight to the complete cloud/star/particle weather pipeline.
    return runtime_settings().style != Style::DarkHour || g_env_light.mSunInitialized;
}

void restore_native_particles() {
    if (!g_savedNativeState) return;
    if (g_env_light.mHousiInitialized && g_env_light.mpHousiPacket != nullptr) {
        JKR_DELETE(g_env_light.mpHousiPacket);
        g_env_light.mpHousiPacket = nullptr;
    }
    g_env_light.mHousiInitialized = false;
    g_env_light.mHousiCount = g_nativeCount;
    g_env_light.field_0xea9 = g_nativeType;
    g_savedNativeState = false;
}

HookAction move_pre(ModContext*, void*, void*, void*) {
    if (active() && !palace_excluded() && runtime_settings().style == Style::DarkHour &&
        !g_env_light.mSunInitialized) {
        g_savedDarkHourVrbox = g_env_light.hide_vrbox;
        g_env_light.hide_vrbox = false;
        g_darkHourVrboxTemporarilyShown = true;
    }
    g_moveVisualScope = native_visual_context();
    if (g_moveVisualScope) boundary::begin_visual_environment();
    if (active()) {
        if (!g_savedNativeState) {
            g_nativeCount = g_env_light.mHousiCount;
            g_nativeType = g_env_light.field_0xea9;
            g_savedNativeState = true;
        }
    } else {
        restore_native_particles();
    }
    return HOOK_CONTINUE;
}

bool enabled() {
    const auto& cfg = runtime_settings();
    return active() && cfg.style != Style::DarkHour;
}

void destroy_packet() {
    if (g_packet != nullptr) {
        JKR_DELETE(g_packet);
        g_packet = nullptr;
    }
}

void move_post(ModContext*, void*, void*, void*) {
    run_trail::move();
    blood::move();
    u8* texture = static_cast<u8*>(dComIfG_getObjectRes("Always", 0x5E));
    if (!enabled() || texture == nullptr) {
        destroy_packet();
        if (g_moveVisualScope) boundary::end_visual_environment();
        g_moveVisualScope = false;
        if (g_darkHourVrboxTemporarilyShown) {
            g_env_light.hide_vrbox = g_savedDarkHourVrbox;
            g_darkHourVrboxTemporarilyShown = false;
        }
        return;
    }
    if (g_packet == nullptr) {
        g_packet = JKR_NEW_ARGS(32) VisualHousiPacket;
        if (g_packet == nullptr) {
            if (g_moveVisualScope) boundary::end_visual_environment();
            g_moveVisualScope = false;
            if (g_darkHourVrboxTemporarilyShown) {
                g_env_light.hide_vrbox = g_savedDarkHourVrbox;
                g_darkHourVrboxTemporarilyShown = false;
            }
            return;
        }
        g_packet->field_0x5de8 = 0.0f;
        g_packet->field_0x10.set(0.0f, 0.0f, 0.0f);
        for (auto& effect : g_packet->mHousiEff) effect.mStatus = 0;
    }
    g_packet->mpResTex = texture;
    dKankyo_housi_Packet* nativePacket = g_env_light.mpHousiPacket;
    const int nativeCount = g_env_light.mHousiCount;
    const u8 nativeType = g_env_light.field_0xea9;
    g_env_light.mpHousiPacket = g_packet;
    g_env_light.mHousiCount = 200;
    g_env_light.field_0xea9 = 0;
    native_particles::move(runtime_settings().style == Style::AstralPlane ? 0.5f : 1.0f);
    g_env_light.mpHousiPacket = nativePacket;
    g_env_light.mHousiCount = nativeCount;
    g_env_light.field_0xea9 = nativeType;
    if (g_moveVisualScope) boundary::end_visual_environment();
    g_moveVisualScope = false;
    if (g_darkHourVrboxTemporarilyShown) {
        g_env_light.hide_vrbox = g_savedDarkHourVrbox;
        g_darkHourVrboxTemporarilyShown = false;
    }
}

HookAction draw_pre(ModContext*, void*, void*, void*) {
    g_drawVisualScope = native_visual_context();
    if (g_drawVisualScope) boundary::begin_visual_environment();
    const bool darkHour = active() && runtime_settings().style == Style::DarkHour;
    const bool customParticlesReady = g_packet != nullptr && g_packet->mpResTex != nullptr;
    // All visual styles own the Twilight-particle decision while active.  The
    // Dark Hour intentionally draws no housi packet at all, so its native
    // packet must still be suppressed even though no replacement exists.
    g_nativeHousiDrawSuppressed = darkHour || customParticlesReady;
    if (g_nativeHousiDrawSuppressed) {
        g_savedHousiInitialized = g_env_light.mHousiInitialized;
        g_env_light.mHousiInitialized = 0;
    }
    // Dark Hour owns the celestial presentation. Native stars and shooting stars can remain
    // initialized from the underlying stage and appear as bright triangular sprites once the
    // Twilight sky context is forced, so keep those packets out of this draw only.
    g_nativeStarDrawSuppressed = darkHour && g_env_light.mStarInitialized;
    if (g_nativeStarDrawSuppressed) {
        g_savedStarInitialized = g_env_light.mStarInitialized;
        g_env_light.mStarInitialized = 0;
    }
    return HOOK_CONTINUE;
}

HookAction cloud_move_pre(ModContext*, void*, void*, void*) {
    g_cloudVisualScope = active() && !palace_excluded();
    if (g_cloudVisualScope) boundary::begin_visual_environment();
    return HOOK_CONTINUE;
}

void cloud_move_post(ModContext*, void*, void*, void*) {
    if (g_cloudVisualScope) boundary::end_visual_environment();
    g_cloudVisualScope = false;
}

HookAction weather_proc_pre(ModContext*, void*, void*, void*) {
    g_weatherProcVisualScope = active() && !palace_excluded();
    if (g_weatherProcVisualScope) boundary::begin_visual_environment();
    return HOOK_CONTINUE;
}

void weather_proc_post(ModContext*, void*, void*, void*) {
    if (g_weatherProcVisualScope) boundary::end_visual_environment();
    g_weatherProcVisualScope = false;
}

void draw_post(ModContext*, void*, void*, void*) {
    run_trail::draw();
    blood::draw();
    if (g_nativeHousiDrawSuppressed) {
        g_env_light.mHousiInitialized = g_savedHousiInitialized;
        g_nativeHousiDrawSuppressed = false;
    }
    if (g_nativeStarDrawSuppressed) {
        g_env_light.mStarInitialized = g_savedStarInitialized;
        g_nativeStarDrawSuppressed = false;
    }
    if (active() && !palace_excluded() && runtime_settings().style == Style::DarkHour &&
        g_env_light.mSunInitialized && g_env_light.mpSunPacket != nullptr) {
        auto* stageInfo = dComIfGp_getStageStagInfo();
        if (stageInfo != nullptr && dStage_stagInfo_GetArg0(stageInfo) == 0) {
            // MFB's ForceMoon bypasses the stage sky-draw flag. Reproduce that
            // queue operation with public vanilla draw-list APIs.
            dComIfGd_setListSky();
            j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(g_env_light.mpSunPacket, 0);
            dComIfGd_setList();
        }
    }
    if (g_packet == nullptr || g_packet->mpResTex == nullptr) {
        if (g_drawVisualScope) boundary::end_visual_environment();
        g_drawVisualScope = false;
        return;
    }
    if (g_env_light.camera_water_in_status != 0) {
        dComIfGd_setXluList2DScreen();
        j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(g_packet, 0);
        dComIfGd_setList();
    } else if (const char* stage = dComIfGp_getStartStageName();
               stage != nullptr && std::strncmp(stage, "D_MN05", 6) == 0) {
        dComIfGd_getOpaListIndScreen()->entryImm(g_packet, 0);
    } else {
        j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(g_packet, 0);
    }
    if (g_drawVisualScope) boundary::end_visual_environment();
    g_drawVisualScope = false;
}
}

ModResult install_hooks() {
    ModResult result = mods::hook::add_pre<RainDraw>(rain_draw_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<WeatherMoveDraw>(move_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<WeatherMoveDraw>(move_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<WeatherMoveDraw2>(cloud_move_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<WeatherMoveDraw2>(cloud_move_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<WeatherProc>(weather_proc_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<WeatherProc>(weather_proc_post);
    if (result != MOD_OK) return result;
    result = mods::hook::add_pre<WeatherDraw>(draw_pre);
    if (result != MOD_OK) return result;
    return mods::hook::add_post<WeatherDraw>(draw_post);
}
void uninstall_hooks() {
    run_trail::clear();
    if (g_nativeHousiDrawSuppressed) {
        g_env_light.mHousiInitialized = g_savedHousiInitialized;
        g_nativeHousiDrawSuppressed = false;
    }
    if (g_nativeStarDrawSuppressed) {
        g_env_light.mStarInitialized = g_savedStarInitialized;
        g_nativeStarDrawSuppressed = false;
    }
    mods::hook::uninstall<WeatherDraw>();
    mods::hook::uninstall<WeatherProc>();
    mods::hook::uninstall<WeatherMoveDraw2>();
    mods::hook::uninstall<WeatherMoveDraw>();
    mods::hook::uninstall<RainDraw>();
    destroy_packet();
    restore_native_particles();
}
void area_reloaded() { g_savedNativeState = false; run_trail::clear(); }
}
