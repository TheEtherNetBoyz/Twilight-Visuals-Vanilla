#include "environment.hpp"

#include "runtime.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_stage.h"

#include <cstring>

namespace twilight_visuals::environment {

namespace {
bool stage_is(const char* name) {
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr && std::strncmp(stage, name, std::strlen(name)) == 0;
}
}  // namespace

bool forest_temple_outside_bridge() {
    return stage_is("D_MN05") && dComIfGp_roomControl_getStayNo() == 4;
}

bool faron_woods() { return stage_is("F_SP108"); }
bool castle_town() { return stage_is("F_SP116"); }
bool kakariko_village() { return stage_is("F_SP109"); }
bool gerudo_desert() { return stage_is("F_SP124"); }

bool reduced_dark_hour_outdoor() {
    return forest_temple_outside_bridge() || faron_woods() || castle_town();
}

float reduced_dark_hour_outdoor_scale() {
    return castle_town() ? 0.68f : 0.56f;
}

bool dark_hour_indoor() {
    if (!visual_effects_active() || runtime_settings().style != Style::DarkHour) return false;
    const char* stageName = dComIfGp_getStartStageName();
    if (stage_is("D_MN08")) {
        const int room = dComIfGp_roomControl_getStayNo();
        return room != 0 && room != 11;
    }
    if (forest_temple_outside_bridge()) return false;

    const bool dungeonStage = stageName != nullptr &&
        (std::strncmp(stageName, "D_MN", 4) == 0 ||
         std::strncmp(stageName, "D_SB", 4) == 0);
    if (dungeonStage) {
        auto* rooms = dComIfGp_getStageRoom();
        const int room = dComIfGp_roomControl_getStayNo();
        if (rooms != nullptr && room >= 0 && room < rooms->num &&
            rooms->m_entries[room] != nullptr) {
            return dStage_roomRead_dt_c_GetVrboxswitch(*rooms->m_entries[room]) == 0;
        }
    }

    auto* stage = dComIfGp_getStage();
    auto* stagInfo = stage != nullptr ? stage->getStagInfo() : nullptr;
    if (stagInfo == nullptr) return false;
    const u32 type = dStage_stagInfo_GetSTType(stagInfo);
    return type == ST_ROOM || type == ST_DUNGEON || type == ST_BOSS_ROOM;
}

}  // namespace twilight_visuals::environment
