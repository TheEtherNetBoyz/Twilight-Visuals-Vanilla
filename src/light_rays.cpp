#include "light_rays.hpp"

#include "environment.hpp"
#include "runtime.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_kankyo.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_camera_mng.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace twilight_visuals::light_rays {
namespace {

struct CachedBeam {
    cXyz source;
    cXyz end;
    f32 width;
    u8 alpha;
};

constexpr int kMaxBeams = 3;
std::array<CachedBeam, kMaxBeams> s_beams{};
int s_beamCount = 0;
int s_cachedRoom = -128;
char s_cachedStage[9]{};
bool s_wasIndoor = false;

cXyz normalized(cXyz value, const cXyz& fallback) {
    const f32 length = value.abs();
    return length > 0.001f ? value / length : fallback;
}

void vertex(const cXyz& position, const GXColor& color) {
    GXPosition3f32(position.x, position.y, position.z);
    GXColor4u8(color.r, color.g, color.b, color.a);
}

void beam_plane(const cXyz& source, const cXyz& end, const cXyz& side,
                f32 sourceWidth, f32 endWidth, GXColor sourceColor, GXColor endColor) {
    const cXyz sourceSide = side * sourceWidth;
    const cXyz endSide = side * endWidth;
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    vertex(source - sourceSide, sourceColor);
    vertex(source + sourceSide, sourceColor);
    vertex(end + endSide, endColor);
    vertex(end - endSide, endColor);
    GXEnd();
}

void draw_beam(const cXyz& source, const cXyz& end, camera_process_class& camera,
               f32 sourceWidth, f32 endWidth, u8 alpha) {
    const cXyz direction = normalized(end - source, cXyz(0.0f, -1.0f, 0.0f));
    const cXyz midpoint = (source + end) * 0.5f;
    const cXyz toCamera = normalized(camera.view.lookat.eye - midpoint,
                                     cXyz(0.0f, 0.0f, 1.0f));
    const cXyz side = normalized(direction.getCrossProduct(toCamera),
                                 cXyz(1.0f, 0.0f, 0.0f));
    const cXyz crossSide = normalized(direction.getCrossProduct(side),
                                      cXyz(0.0f, 0.0f, 1.0f));

    // Match the outdoor Dark Hour green inside the shafts only. The room's
    // ambient palette remains blue.
    const GXColor outerSource{24, 172, 48, static_cast<u8>(alpha / 2)};
    const GXColor outerEnd{22, 158, 44, static_cast<u8>((alpha * 3) / 8)};
    const GXColor coreSource{46, 236, 76, alpha};
    const GXColor coreEnd{38, 218, 68, static_cast<u8>((alpha * 3) / 4)};

    beam_plane(source, end, side, sourceWidth * 1.8f, endWidth * 1.8f,
               outerSource, outerEnd);
    beam_plane(source, end, crossSide, sourceWidth * 1.35f, endWidth * 1.35f,
               outerSource, outerEnd);
    beam_plane(source, end, side, sourceWidth, endWidth, coreSource, coreEnd);
    beam_plane(source, end, crossSide, sourceWidth * 0.72f, endWidth * 0.72f,
               coreSource, coreEnd);
}

bool line_hit(const cXyz& start, const cXyz& end, cXyz& hit) {
    dBgS_LinChk check;
    check.Set(&start, &end, nullptr);
    if (!dComIfG_Bgsp().LineCross(&check)) return false;
    hit = check.GetCross();
    return true;
}

void rebuild_room_layout(const char* stage, int room, const cXyz& playerPosition,
                         s16 playerFacing) {
    s_beamCount = 0;
    s_cachedRoom = room;
    std::strncpy(s_cachedStage, stage != nullptr ? stage : "", sizeof(s_cachedStage) - 1);
    s_cachedStage[sizeof(s_cachedStage) - 1] = '\0';

    // A room transition gives us a genuine architectural opening: the door
    // Link just crossed. Capture it once and cast a broad, soft spill inward.
    // These anchors remain fixed until the stage or room changes.
    const f32 sine = cM_ssin(playerFacing);
    const f32 cosine = cM_scos(playerFacing);
    const cXyz forward(sine, -0.075f, cosine);
    const cXyz right(cosine, 0.0f, -sine);
    static constexpr f32 lateralOffsets[] = {-105.0f, 105.0f};

    for (f32 lateral : lateralOffsets) {
        CachedBeam& beam = s_beams[s_beamCount++];
        beam.source = playerPosition - cXyz(sine * 115.0f, -155.0f, cosine * 115.0f)
                      + right * lateral;
        cXyz desiredEnd = beam.source + forward * 1050.0f;
        cXyz wallHit;
        if (line_hit(beam.source, desiredEnd, wallHit)) {
            desiredEnd = wallHit - normalized(forward, cXyz(0.0f, 0.0f, 1.0f)) * 18.0f;
        }
        beam.end = desiredEnd;
        beam.width = 105.0f;
        beam.alpha = 54;
    }
}

class IndoorLightRayPacket final : public J3DPacket {
public:
    void draw() override {
        if (!visual_effects_active() || !environment::dark_hour_indoor()) return;
        auto* player = dComIfGp_getLinkPlayer();
        auto* camera = static_cast<camera_process_class*>(dComIfGp_getCamera(0));
        if (player == nullptr || camera == nullptr || dComIfGd_getView() == nullptr) return;

        j3dSys.reinitGX();
        GXSetNumChans(1);
        GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
                     GX_LIGHT_NULL, GX_DF_CLAMP, GX_AF_NONE);
        GXSetNumTexGens(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                        GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
                        GX_TRUE, GX_TEVPREV);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
        GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_DISABLE);
        GXSetZCompLoc(GX_TRUE);
        GXSetCullMode(GX_CULL_NONE);
        GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
        GXSetNumIndStages(0);
        GXClearVtxDesc();
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);
        // The room fog already provides the atmospheric fade. Applying it a
        // second time to these translucent shafts erased them in dark rooms.
        GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, GXColor{0, 0, 0, 0});

        const char* stage = dComIfGp_getStartStageName();
        const int room = dComIfGp_roomControl_getStayNo();
        if (!s_wasIndoor || room != s_cachedRoom || stage == nullptr ||
            std::strncmp(s_cachedStage, stage, sizeof(s_cachedStage)) != 0) {
            rebuild_room_layout(stage, room, player->current.pos, player->shape_angle.y);
        }
        s_wasIndoor = true;

        for (int i = 0; i < s_beamCount; ++i) {
            const CachedBeam& beam = s_beams[i];
            // Keep the shaft vivid throughout tall rooms. It ends above the
            // floor rather than fading over its full height, which previously
            // made the useful portion near Link nearly invisible.
            const cXyz ray = beam.end - beam.source;
            const f32 rayLength = ray.abs();
            const f32 stopShort = std::min(110.0f / std::max(rayLength, 1.0f), 0.18f);
            const cXyz visibleEnd = beam.source + ray * (1.0f - stopShort);
            draw_beam(beam.source, visibleEnd, *camera, beam.width,
                      beam.width * 0.82f, beam.alpha);
        }

        J3DShape::resetVcdVatCache();
    }
};

IndoorLightRayPacket s_packet;

}  // namespace

void draw() {
    if (!visual_effects_active() || !environment::dark_hour_indoor()) {
        s_wasIndoor = false;
        return;
    }
    dComIfGd_setXluListBG();
    j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(&s_packet, 0);
    dComIfGd_setList();
}

}  // namespace twilight_visuals::light_rays
