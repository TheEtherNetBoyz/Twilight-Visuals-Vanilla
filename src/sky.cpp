#include "sky.hpp"
#include "d/d_com_inf_game.h"
#include <cstring>
#include <cstdio>
#include <memory>
#include <chrono>

namespace twilight_visuals::sky {
namespace {
constexpr char archive[] = "Stg_00";
char source[32]{};
std::unique_ptr<dRes_info_c> resource;
std::chrono::steady_clock::time_point retryAfter{};
// This archive belongs to the mod, not the live stage's resource table.
dStage_nodeHeader* find(dStage_fileHeader* file, const char* tag) {
    if (!file) return nullptr;
    for (int i = 0; i < file->m_chunkCount; ++i) {
        auto& node = file->m_nodes[i];
        if (std::memcmp(&node.m_tag, tag, 4) == 0 && node.m_entryNum > 0) {
            node.m_offset.setBase(file);
            return &node;
        }
    }
    return nullptr;
}
dStage_nodeHeader* layer_node(dStage_fileHeader* file, const char* prefix, int layer) {
    char tag[4]{prefix[0], prefix[1], prefix[2], char(layer + (layer < 10 ? '0' : 'W'))};
    return find(file, tag);
}
bool colors(dStage_fileHeader* file, int layer, int slot, stage_vrboxcol_info_class& result) {
    auto* env = layer_node(file, "Env", layer);
    auto* col = layer_node(file, "Col", layer);
    auto* pal = layer_node(file, "PAL", layer);
    auto* vrb = layer_node(file, "VRB", layer);
    if (!env || !col || !pal || !vrb) return false;
    auto* environments = (stage_envr_info_class*)env->m_offset;
    auto* selections = (stage_pselect_info_class*)col->m_offset;
    auto* palettes = (stage_palette_info_class*)pal->m_offset;
    const auto selection = environments[0].pselect_id[0];
    if (selection >= col->m_entryNum) return false;
    const auto palette = selections[selection].palette_id[slot];
    if (palette >= pal->m_entryNum) return false;
    const auto box = palettes[palette].vrboxcol_id;
    if (box >= vrb->m_entryNum) return false;
    result = ((stage_vrboxcol_info_class*)vrb->m_offset)[box];
    return true;
}
}
void shutdown() {
    resource.reset();
    retryAfter = {};
    source[0] = 0;
}
bool read(VisualSkybox* out, const char* stage, u8 layer, u8 slot, u8 minimum) {
    if (!out || !stage || !*stage || std::strlen(stage) >= sizeof(source) ||
        layer >= 15 || slot >= 6 || minimum >= 15) return false;
    if (std::strcmp(source, stage)) {
        shutdown();
        std::strcpy(source, stage);
    }
    if (!resource) {
        if (std::chrono::steady_clock::now() < retryAfter) return false;
        char path[64];
        std::snprintf(path, sizeof(path), "/res/Stage/%s/", stage);
        resource = std::make_unique<dRes_info_c>();
        if (!resource->set(archive, path, mDoDvd_MOUNT_DIRECTION_TAIL, nullptr)) {
            resource.reset();
            retryAfter = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            return false;
        }
        resource->incCount();
    }
    const int sync = resource->setRes();
    if (sync != 0) {
        if (sync < 0) {
            resource.reset();
            retryAfter = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        }
        return false;
    }
    auto* file = (dStage_fileHeader*)dRes_control_c::getRes(archive, "stage.dzs", resource.get(), 1);
    auto* evly = find(file, "EVLY");
    if (!evly) return false;
    auto* entries = (dStage_Elst_dt_c*)evly->m_offset;
    stage_vrboxcol_info_class value{};
    bool found = false;
    // Some rooms map to an empty palette layer. Continue to a complete one.
    for (int i = 0; i < evly->m_entryNum; ++i) {
        const int candidate = entries[i].m_layerTable[layer];
        if (candidate >= minimum && candidate < 15 && colors(file, candidate, slot, value)) {
            found = true;
            break;
        }
    }
    if (!found) return false;
    out->sky = {value.sky_col.r, value.sky_col.g, value.sky_col.b};
    out->cloudTop = {value.kumo_top_col.r, value.kumo_top_col.g, value.kumo_top_col.b};
    out->cloudBottom = {value.kumo_bottom_col.r, value.kumo_bottom_col.g, value.kumo_bottom_col.b};
    out->cloudShadow = {value.kumo_shadow_col.r, value.kumo_shadow_col.g, value.kumo_shadow_col.b, value.kumo_shadow_col.a};
    out->hazeOuter = {value.kasumi_outer_col.r, value.kasumi_outer_col.g, value.kasumi_outer_col.b, value.kasumi_outer_col.a};
    out->hazeInner = {value.kasumi_inner_col.r, value.kasumi_inner_col.g, value.kasumi_inner_col.b, value.kasumi_inner_col.a};
    return true;
}
bool select_layer(int layer, int minimum) {
    auto* stage = dComIfGp_getStage();
    if (!stage || layer < 0 || layer >= 15) return false;
    auto* elst = stage->getElst();
    int room = dComIfGp_roomControl_getStayNo();
    if (!elst || !elst->m_entries || room < 0 || room >= elst->m_entryNum) return false;
    const int selected = elst->m_entries[room].m_layerTable[layer];
    if (selected < minimum || selected >= 15) return false;
    auto* file = (dStage_fileHeader*)dComIfG_getStageRes("stage.dzs");
    auto* env = layer_node(file, "Env", selected);
    auto* col = layer_node(file, "Col", selected);
    auto* pal = layer_node(file, "PAL", selected);
    auto* vrb = layer_node(file, "VRB", selected);
    auto* light = layer_node(file, "LGT", selected);
    if (!env || !col || !pal || !vrb) return false;
    stage->setEnvrInfo((stage_envr_info_class*)env->m_offset);
    stage->setPselectInfo((stage_pselect_info_class*)col->m_offset);
    stage->setPaletteInfo((stage_palette_info_class*)pal->m_offset);
    stage->setVrboxcolInfo((stage_vrboxcol_info_class*)vrb->m_offset);
    stage->setLightVecInfo(light ? (stage_pure_lightvec_info_class*)light->m_offset : nullptr);
    stage->setLightVecInfoNum(light ? int(light->m_entryNum) : 0);
    return true;
}
}
