#include "run_trail.hpp"
#include "running.hpp"
#include "runtime.hpp"
#include "interpolation.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/actor/d_a_alink.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "SSystem/SComponent/c_math.h"

namespace twilight_visuals::run_trail {
namespace {
constexpr unsigned lifetime = 12;
struct Strand {
    float side, height, behind, width;
    GXColor color;
};
constexpr Strand strands[] = {
    {-22, 65, 18, 1.0f, {105, 205, 255, 255}},
    { 22, 90, 18, 1.0f, {105, 205, 255, 255}},
};
constexpr unsigned strandCount = sizeof(strands) / sizeof(strands[0]);
constexpr unsigned segmentCount = strandCount * lifetime;
struct Segment {
    cXyz from, to;
    unsigned age = lifetime;
    unsigned strand = 0;
};
class TrailPacket final : public J3DPacket {
public:
    Segment segments[segmentCount];
    unsigned next = 0;
    void draw() override {
        if (!active() || !runtime_settings().skywardSwordRunning) return;
        j3dSys.reinitGX();
        GXSetNumChans(1);
        GXSetChanCtrl(GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX,
                      GX_LIGHT_NULL, GX_DF_CLAMP, GX_AF_NONE);
        GXSetNumTexGens(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
        GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
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
        dKy_GxFog_set();
        // Camera-up keeps the narrow ribbons visible without line-width state.
        const auto view = j3dSys.getViewMtx();
        for (const auto& s : segments) {
            if (s.age >= lifetime) continue;
            const float fade = 1.0f - static_cast<float>(s.age) / lifetime;
            const float tailFade = 1.0f - static_cast<float>(s.age + 1) / lifetime;
            const auto vertex = [&](const cXyz& p, float side, float f, bool core) {
                const auto& strand = strands[s.strand];
                const float width = (core ? 1.0f : 3.0f) * strand.width * f * side;
                GXPosition3f32(p.x + view[1][0] * width,
                               p.y + view[1][1] * width,
                               p.z + view[1][2] * width);
                GXColor4u8(core ? 105 : 25, core ? 205 : 115, 255,
                           static_cast<u8>((core ? 210 : 90) * f * f));
            };
            for (int layer = 0; layer < 2; ++layer) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                vertex(s.from, -1, tailFade, layer != 0);
                vertex(s.from, 1, tailFade, layer != 0);
                vertex(s.to, 1, fade, layer != 0);
                vertex(s.to, -1, fade, layer != 0);
                GXEnd();
            }
        }
        J3DShape::resetVcdVatCache();
    }
};
TrailPacket packet;
cXyz previous[strandCount];
cXyz previousPosition;
bool connected = false;
daAlink_c* owner = nullptr;
}

void clear() {
    for (auto& segment : packet.segments) segment.age = lifetime;
    packet.next = 0;
    connected = false;
    owner = nullptr;
}

void move() {
    if (!dusk::frame_interp::is_sim_frame()) return;
    if (!active() || !runtime_settings().skywardSwordRunning) { clear(); return; }
    for (auto& segment : packet.segments)
        if (segment.age < lifetime) ++segment.age;
    auto* p = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (!p || p->checkEventRun()) { clear(); return; }
    if (!running::is_running()) { connected = false; return; }
    if (owner != p || (connected && (p->current.pos - previousPosition).abs2() > 40000.0f))
        clear();
    owner = p;
    const float sine = cM_ssin(p->shape_angle.y);
    const float cosine = cM_scos(p->shape_angle.y);
    for (unsigned i = 0; i < strandCount; ++i) {
        const auto& strand = strands[i];
        const cXyz point(p->current.pos.x + cosine * strand.side - sine * strand.behind,
                         p->current.pos.y + strand.height,
                         p->current.pos.z - sine * strand.side - cosine * strand.behind);
        if (connected && (p->current.pos - previousPosition).abs2() > 1.0f) {
            auto& segment = packet.segments[packet.next];
            packet.next = (packet.next + 1) % segmentCount;
            segment.from = previous[i];
            segment.to = point;
            segment.age = 0;
            segment.strand = i;
        }
        previous[i] = point;
    }
    previousPosition = p->current.pos;
    connected = true;
}

void draw() {
    if (!active() || !runtime_settings().skywardSwordRunning) return;
    for (const auto& segment : packet.segments) {
        if (segment.age < lifetime) {
            j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu)->entryImm(&packet, 0);
            break;
        }
    }
}
}
