#include "postprocess.hpp"
#include "runtime.hpp"
#include "mods/service.hpp"
#include "hook_api.hpp"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "SSystem/SComponent/c_m3d.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace twilight_visuals::postprocess {
namespace {
DEFINE_HOOK(&mDoGph_gInf_c::bloom_c::draw, BloomDraw);

struct BloomState {
    mDoGph_gInf_c::bloom_c* bloom{};
    GXColor blend{};
    GXColor mono{};
    u8 enable{};
    u8 mode{};
    u8 point{};
    u8 blurSize{};
    u8 blurRatio{};
    bool saved{};
};

BloomState s_bloomState;
void restore_bloom_state();

bool palace_dark_hour() {
    const char* stage = dComIfGp_getStartStageName();
    return runtime_settings().style == Style::DarkHour && stage != nullptr &&
           std::strncmp(stage, "D_MN08", 6) == 0;
}

void draw_legacy_bloom(mDoGph_gInf_c::bloom_c* bloom) {
    const bool enabled = bloom->mEnable && bloom->m_buffer != nullptr;
    if (bloom->mMonoColor.a == 0 && !enabled) return;

    const f32 width = JUTVideo::getManager()->getRenderWidth();
    const f32 height = JUTVideo::getManager()->getRenderHeight();
    GXSetViewportRender(0.0f, 0.0f, width, height, 0.0f, 1.0f);
    GXSetScissorRender(0, 0, width, height);
    GXLoadTexObj(mDoGph_gInf_c::getFrameBufferTexObj(), GX_TEXMAP0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c);
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_GREEN);
    GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetZCompLoc(1);
    GXSetZMode(0, GX_ALWAYS, 0);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, g_clearColor);
    GXSetFogRangeAdj(0, 0, 0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetDither(1);

    Mtx44 ortho;
    C_MTXOrtho(ortho, 0.0f, 4.0f, 0.0f, 4.0f, 0.0f, 10.0f);
    GXLoadPosMtxImm(cMtx_getIdentity(), 0);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);

    if (bloom->mMonoColor.a != 0) {
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C2, GX_CC_ZERO);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                        GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A2);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                        GX_TEVPREV);
        GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP1);
        GXSetTevColor(GX_TEVREG2, bloom->mMonoColor);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_OR);
        mDoGph_drawFilterQuad(4, 4);
    }

    if (!enabled) return;

    GXCreateFrameBuffer(width, height);
    GXSetNumTevStages(3);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_TEXA, GX_CC_HALF, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP1);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_TEXC, GX_CC_CPREV, GX_CC_HALF, GX_CC_C0);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP3, GX_TEV_SWAP3);
    GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_CPREV, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_OR);
    const s16 bloom_alpha = static_cast<s16>(0x40 *
        std::clamp(runtime_settings().bloomBrightness, 0.0f, 1.0f));
    GXSetTevColorS10(GX_TEVREG0,
        {static_cast<s16>(-bloom->mPoint), static_cast<s16>(-bloom->mPoint),
         static_cast<s16>(-bloom->mPoint), bloom_alpha});
    GXSetTevColor(GX_TEVREG1,
        {bloom->mBlureRatio, bloom->mBlureRatio, bloom->mBlureRatio, bloom->mBlureRatio});
    GXPixModeSync();
    mDoGph_drawFilterQuad(2, 2);

    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);

    void* zbuffer = mDoGph_gInf_c::getZbufferTex();
    GXSetTexCopySrc(0, 0, width / 2, height / 2);
    GXSetTexCopyDst(width / 4, height / 4, GX_TF_RGBA8, GX_TRUE);
    GXCopyTex(zbuffer, GX_FALSE);

    TGXTexObj quarter;
    GXInitTexObj(&quarter, zbuffer, width / 4, height / 4, GX_TF_RGBA8,
                 GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&quarter, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f,
                    GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&quarter, GX_TEXMAP0);

    GXSetNumTexGens(8);
    u32 matrix = 0x1e;
    int angle = 0;
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c);
    for (int tex_coord = static_cast<int>(GX_TEXCOORD1);
         tex_coord < static_cast<int>(GX_MAX_TEXCOORD); ++tex_coord) {
        GXSetTexCoordGen(static_cast<GXTexCoordID>(tex_coord), GX_TG_MTX2x4, GX_TG_TEX0, matrix);
        const f32 blur = bloom->mBlureSize * ((448.0f / height) / 6400.0f);
        mDoMtx_stack_c::transS((blur * cM_scos(angle)) * mDoGph_gInf_c::getInvScale(),
                               blur * cM_ssin(angle), 0.0f);
        GXLoadTexMtxImm(mDoMtx_stack_c::get(), matrix, GX_MTX2x4);
        matrix += 3;
        angle += 0x2492;
    }

    GXSetNumTevStages(8);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_A1, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    for (int stage = static_cast<int>(GX_TEVSTAGE1); stage < 8; ++stage) {
        const auto tev_stage = static_cast<GXTevStageID>(stage);
        GXSetTevOrder(tev_stage, static_cast<GXTexCoordID>(stage), GX_TEXMAP0, GX_COLOR_NULL);
        GXSetTevColorIn(tev_stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_A1, GX_CC_CPREV);
        GXSetTevColorOp(tev_stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(tev_stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
        GXSetTevAlphaOp(tev_stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    }
    GXPixModeSync();
    mDoGph_drawFilterQuad(1, 1);

    GXSetTexCopySrc(0, 0, width / 4, height / 4);
    GXSetTexCopyDst(width / 8, height / 8, GX_TF_RGBA8, GX_TRUE);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_OR);
    GXPixModeSync();
    GXInvalidateTexAll();
    GXCopyTex(zbuffer, GX_FALSE);

    TGXTexObj eighth;
    GXInitTexObj(&eighth, zbuffer, width / 8, height / 8, GX_TF_RGBA8,
                 GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&eighth, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f,
                    GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&eighth, GX_TEXMAP0);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_OR);
    GXPixModeSync();
    GXInvalidateTexAll();
    mDoGph_drawFilterQuad(1, 1);
    eighth.reset();

    GXSetTexCopySrc(0, 0, width / 4, height / 4);
    GXSetTexCopyDst(width / 4, height / 4, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(zbuffer, GX_FALSE);
    GXRestoreFrameBuffer();

    GXLoadTexObj(&quarter, GX_TEXMAP0);
    GXSetTevColor(GX_TEVREG0, bloom->mBlendColor);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_BLEND, bloom->mMode == 1 ? GX_BL_INVDSTCLR : GX_BL_ONE,
                   GX_BL_SRCALPHA, GX_LO_OR);
    GXPixModeSync();
    GXInvalidateTexAll();
    mDoGph_drawFilterQuad(4, 4);
}

u8 scaled_u8(u8 value, f32 scale) {
    return static_cast<u8>(std::clamp(static_cast<int>(value * scale + 0.5f), 0, 255));
}

HookAction bloom_draw_pre(ModContext*, void* args, void*, void*) {
    auto* bloom = mods::arg<mDoGph_gInf_c::bloom_c*>(args, 0);
    if (bloom == nullptr || !active()) {
        return HOOK_CONTINUE;
    }

    BloomMode mode = runtime_settings().bloomMode;
    // Preserve the old config variable for users upgrading from the first vanilla port.
    if (mode == BloomMode::Native && runtime_settings().legacyBloom) {
        mode = BloomMode::Classic;
    }
    if (mode == BloomMode::Native) {
        return HOOK_CONTINUE;
    }

    // The override is entirely local to this hook. Dusklight's bloom object is restored before
    // returning so the setting never permanently mutates the host's bloom state.
    s_bloomState = {bloom, bloom->mBlendColor, bloom->mMonoColor, bloom->mEnable,
                    bloom->mMode, bloom->mPoint, bloom->mBlureSize,
                    bloom->mBlureRatio, true};

    if (mode == BloomMode::Off) {
        bloom->mEnable = 0;
        bloom->mMonoColor.a = 0;
        restore_bloom_state();
        return HOOK_SKIP_ORIGINAL;
    }

    f32 gain = std::clamp(runtime_settings().bloomBrightness, 0.0f, 1.0f);
    if (palace_dark_hour()) gain *= 0.55f;
    bloom->mEnable = 1;
    bloom->mBlendColor.a = scaled_u8(bloom->mBlendColor.a, gain);

    if (mode == BloomMode::Classic) {
        draw_legacy_bloom(bloom);
        restore_bloom_state();
        return HOOK_SKIP_ORIGINAL;
    }

    if (mode == BloomMode::Dusklight) {
        // draw2() is the vanilla Dusklight pyramid bloom implementation exposed by the
        // game-side bloom object. Calling it from the vanilla draw hook avoids changing
        // Dusklight's source while still overriding its selected mode for this preset.
        bloom->draw2();
        restore_bloom_state();
        return HOOK_SKIP_ORIGINAL;
    }

    restore_bloom_state();
    return HOOK_CONTINUE;
}

void restore_bloom_state() {
    if (!s_bloomState.saved || s_bloomState.bloom == nullptr) return;
    auto* bloom = s_bloomState.bloom;
    bloom->mBlendColor = s_bloomState.blend;
    bloom->mMonoColor = s_bloomState.mono;
    bloom->mEnable = s_bloomState.enable;
    bloom->mMode = s_bloomState.mode;
    bloom->mPoint = s_bloomState.point;
    bloom->mBlureSize = s_bloomState.blurSize;
    bloom->mBlureRatio = s_bloomState.blurRatio;
    s_bloomState = {};
}

void draw_astral_chromatic_aberration() {
    const auto& cfg = runtime_settings();
    const char* stage = dComIfGp_getStartStageName();
    if (!active() || cfg.style != Style::AstralPlane || stage == nullptr ||
        std::strncmp(stage, "D_MN08", 6) == 0) return;
    const f32 strength = std::clamp(cfg.chromaticAberration, 0, 200) / 100.0f;
    const u16 width = mDoGph_gInf_c::getWidth();
    const u16 height = mDoGph_gInf_c::getHeight();
    if (strength == 0.0f || width == 0 || height == 0) return;

    static std::vector<u8> pixels;
    pixels.resize(GXGetTexBufferSize(width, height, GX_TF_RGBA8, GX_FALSE, 0));
    GXSetTexCopySrc(0, 0, width, height);
    GXSetTexCopyDst(width, height, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(pixels.data(), GX_FALSE);
    GXPixModeSync();
    GXInvalidateTexAll();
    TGXTexObj scene;
    GXInitTexObj(&scene, pixels.data(), width, height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&scene, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
    j3dSys.reinitGX();
    GXLoadTexObj(&scene, GX_TEXMAP0);
    GXSetViewport(0, 0, width, height, 0, 1);
    GXSetScissor(0, 0, width, height);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetZCompLoc(GX_TRUE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetColorUpdate(GX_TRUE);
    GXSetFog(GX_FOG_NONE, 0, 0, 0, 0, g_clearColor);
    GXSetCullMode(GX_CULL_NONE);
    Mtx44 ortho;
    C_MTXOrtho(ortho, 0, 1, 0, 1, 0, 10);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    GXLoadPosMtxImm(cMtx_getIdentity(), 0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    const GXColor masks[] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};
    for (int channel = 0; channel < 3; ++channel) {
        const f32 inset = (channel - 1) * strength * 0.006f;
        GXSetTevColor(GX_TEVREG0, masks[channel]);
        GXSetBlendMode(channel == 0 ? GX_BM_NONE : GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_COPY);
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0, 0, 0); GXTexCoord2f32(inset, inset);
        GXPosition3f32(1, 0, 0); GXTexCoord2f32(1 - inset, inset);
        GXPosition3f32(1, 1, 0); GXTexCoord2f32(1 - inset, 1 - inset);
        GXPosition3f32(0, 1, 0); GXTexCoord2f32(inset, 1 - inset);
        GXEnd();
    }
    GXSetAlphaUpdate(GX_TRUE);
    j3dSys.reinitGX();
    J3DShape::resetVcdVatCache();
}

void bloom_draw_post(ModContext*, void*, void*, void*) {
    restore_bloom_state();
    draw_astral_chromatic_aberration();
}
}

ModResult install_hooks() {
    ModResult result = mods::hook::add_pre<BloomDraw>(bloom_draw_pre);
    if (result != MOD_OK) return result;
    result = mods::hook::add_post<BloomDraw>(bloom_draw_post);
    if (result != MOD_OK) mods::hook::uninstall<BloomDraw>();
    return result;
}
void uninstall_hooks() { mods::hook::uninstall<BloomDraw>(); }
}
