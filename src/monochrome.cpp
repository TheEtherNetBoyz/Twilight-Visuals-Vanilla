#include "monochrome.hpp"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"
#include "m_Do/m_Do_mtx.h"
namespace twilight_visuals {
static void captureFullFrameBuffer() {
    GXSetTexCopySrc(0, 0, mDoGph_gInf_c::getWidth(), mDoGph_gInf_c::getHeight());
    GXSetTexCopyDst(mDoGph_gInf_c::getWidth(), mDoGph_gInf_c::getHeight(),
                    (GXTexFmt)mDoGph_gInf_c::m_fullFrameBufferTimg->format, 0);
    GXCopyTex(mDoGph_gInf_c::m_fullFrameBufferTex, 0);
    GXPixModeSync();
    GXInvalidateTexAll();
}

static void drawFullFrameBuffer(bool mirror, bool monochrome = false) {
#if TARGET_PC
    mDoGph_gInf_c::m_fullFrameBufferTexObj.reset();
#endif
    mDoLib_setResTimgObj(mDoGph_gInf_c::m_fullFrameBufferTimg,
                         &mDoGph_gInf_c::m_fullFrameBufferTexObj, 0, NULL);
    GXLoadTexObj(&mDoGph_gInf_c::m_fullFrameBufferTexObj, GX_TEXMAP0);

    GXSetNumChans(0);
    GXSetNumIndStages(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3C);
    if (monochrome) {
        // Build approximate luminance (R/4 + G/2 + B/4) from the captured
        // environment so its textures, rather than only its lighting, lose color.
        GXSetNumTevStages(3);
        GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
        GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
        GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
        GXColorS10 quarter = {64, 64, 64, 0};
        GXSetTevColorS10(GX_TEVREG2, quarter);

        for (int stage = 0; stage < 3; ++stage) {
            GXTevStageID stageId = static_cast<GXTevStageID>(GX_TEVSTAGE0 + stage);
            GXSetTevOrder(stageId, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
            GXSetTevSwapMode(stageId, GX_TEV_SWAP0,
                             static_cast<GXTevSwapSel>(GX_TEV_SWAP1 + stage));
            GXSetTevAlphaIn(stageId, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
            GXSetTevAlphaOp(stageId, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE,
                            GX_TEVPREV);
        }
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C2, GX_CC_ZERO);
        GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF, GX_CC_CPREV);
        GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C2, GX_CC_CPREV);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE,
                        GX_TEVPREV);
        GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE,
                        GX_TEVPREV);
        GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE,
                        GX_TEVPREV);
    } else {
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE,
                        GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE,
                        GX_TEVPREV);
    }
    GXSetZCompLoc(GX_ENABLE);
    GXSetZMode(GX_DISABLE, GX_ALWAYS, GX_DISABLE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, GXColor{0,0,0,0});
    GXSetFogRangeAdj(GX_DISABLE, 0, NULL);
    GXSetCullMode(GX_CULL_NONE);
    GXSetDither(GX_ENABLE);

    Mtx44 mtx;
    if (mirror) {
        MTXOrtho(mtx, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 10.0f);
    } else {
        MTXOrtho(mtx, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 10.0f);
    }
    GXSetProjection(mtx, GX_ORTHOGRAPHIC);
    GXLoadPosMtxImm(cMtx_getIdentity(), GX_PNMTX0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_RGB8, 0);
    mDoGph_drawFilterQuad(1, 1);
}
void draw_monochrome_background() {
    if (!mDoGph_gInf_c::m_fullFrameBufferTimg || !mDoGph_gInf_c::m_fullFrameBufferTex) return;
    captureFullFrameBuffer();
    drawFullFrameBuffer(false, true);
}
}
