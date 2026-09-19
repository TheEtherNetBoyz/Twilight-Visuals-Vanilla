#include "load_acceleration.hpp"

#include "hook_api.hpp"
#include "runtime.hpp"

#include "JSystem/JUtility/JUTFader.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_overlap.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_overlap_req.h"
#include "f_op/f_op_scene_req.h"
#include "f_pc/f_pc_base.h"
#include "f_pc/f_pc_create_req.h"
#include "f_pc/f_pc_create_tag.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_method.h"
#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_node_req.h"
#include "m_Do/m_Do_Reset.h"

#include <algorithm>
#include <cstring>

namespace twilight_visuals::load_acceleration {
namespace {
DEFINE_HOOK(&cPhs_Do, PhaseDo);
DEFINE_HOOK(&fpcBs_SubCreate, BaseSubCreate);
DEFINE_HOOK(&fpcNdRq_Execute, NodeRequestExecute);
DEFINE_HOOK(&fpcCtRq_Handler, CreateRequestHandler);
DEFINE_HOOK(&fopScnRq_Request, SceneRequest);
DEFINE_HOOK_SYMBOL("fopOvlpReq_SetPeektime", void(overlap_request_class*, u16), SetPeektime);
DEFINE_HOOK(&JUTFader::startFadeIn, FaderStartIn);
DEFINE_HOOK(&JUTFader::startFadeOut, FaderStartOut);
DEFINE_HOOK_SYMBOL("fopScnRq_phase_Execute", cPhs_Step(scene_request_class*), ScenePhaseExecute);

class overlap1_class : public overlap_task_class {
public:
    int field_0xcc;
    int field_0xd0;
    int field_0xd4;
};

DEFINE_HOOK_SYMBOL("dOvlpFd_FadeIn", int(overlap1_class*), OverlapFadeIn);
DEFINE_HOOK_SYMBOL("dOvlpFd_FadeOut", int(overlap1_class*), OverlapFadeOut);

bool transitionActive = false;
bool transitionSawNextStage = false;
bool titleToFileSelect = false;

LoadMode mode() { return runtime_settings().loadMode; }
bool accelerated() { return mode() != LoadMode::Normal; }

bool resetting() {
    return mDoRst::isReset() || mDoRst::isReturnToMenu() || mDoRst::isShutdown();
}

bool zant_death_transition() {
    return dComIfGp_isEnableNextStage() &&
           std::strcmp(dComIfGp_getNextStageName(), "D_MN08A") == 0 &&
           dComIfGp_getNextStageRoomNo() == 10 &&
           dComIfGp_getNextStageLayer() == 9;
}

bool dungeon_seven_fast_exception() {
    return mode() == LoadMode::Fast && !resetting() &&
           std::strcmp(dComIfGp_getStartStageName(), "D_MN07") == 0 &&
           std::strcmp(dComIfGp_getNextStageName(), "D_MN07") == 0;
}

bool protected_transition() {
    return resetting() || titleToFileSelect || zant_death_transition() ||
           dungeon_seven_fast_exception();
}

bool gameplay_fast_transition(s16 processName) {
    if (processName != fpcNm_PLAY_SCENE_e || protected_transition()) return false;

    // MFB deliberately leaves boot/title transitions on their original wipe path.
    if (fpcM_SearchByName(fpcNm_OPENING_SCENE_e) != nullptr ||
        fpcM_SearchByName(fpcNm_TITLE_e) != nullptr)
    {
        return false;
    }

    return true;
}

void phase_do_replace(ModContext*, void* args, void* retval, void*) {
    auto* phase = mods::arg<request_of_phase_process_class*>(args, 0);
    void* data = mods::arg<void*>(args, 1);
    int result = cPhs_INIT_e;
    const int maxPasses = accelerated() ? 64 : 1;

    for (int pass = 0; pass < maxPasses; ++pass) {
        cPhs__Handler* beforeTable = phase != nullptr ? phase->mpHandlerTable : nullptr;
        const int beforeId = phase != nullptr ? phase->id : 0;
        result = PhaseDo::g_orig(phase, data);
        if (!accelerated() || phase == nullptr || result == cPhs_COMPLEATE_e ||
            result == cPhs_ERROR_e || result == cPhs_UNK3_e ||
            phase->mpHandlerTable == nullptr)
        {
            break;
        }
        if (phase->mpHandlerTable == beforeTable && phase->id == beforeId) break;
    }

    if (retval != nullptr) *static_cast<int*>(retval) = result;
}

void base_sub_create_replace(ModContext*, void* args, void* retval, void*) {
    auto* process = mods::arg<base_process_class*>(args, 0);
    if (!accelerated()) {
        const int result = BaseSubCreate::g_orig(process);
        if (retval != nullptr) *static_cast<int*>(retval) = result;
        return;
    }

    int result = cPhs_INIT_e;
    const int maxPasses = 4;
    for (int pass = 0; pass < maxPasses; ++pass) {
        const int phase = fpcMtd_Create(process->methods, process);
        switch (phase) {
        case cPhs_NEXT_e:
            continue;
        case cPhs_COMPLEATE_e:
            fpcBs_DeleteAppend(process);
            process->state.create_phase = cPhs_NEXT_e;
            result = cPhs_NEXT_e;
            break;
        case cPhs_INIT_e:
        case cPhs_LOADING_e:
            process->state.init_state = 1;
            process->state.create_phase = cPhs_INIT_e;
            result = cPhs_INIT_e;
            break;
        case cPhs_UNK3_e:
            process->state.create_phase = cPhs_UNK3_e;
            result = cPhs_UNK3_e;
            break;
        case cPhs_ERROR_e:
        default:
            process->state.create_phase = cPhs_ERROR_e;
            result = cPhs_ERROR_e;
            break;
        }
        break;
    }
    if (result == cPhs_INIT_e) {
        process->state.init_state = 1;
        process->state.create_phase = cPhs_INIT_e;
    }
    if (retval != nullptr) *static_cast<int*>(retval) = result;
}

void node_request_execute_replace(ModContext*, void* args, void* retval, void*) {
    auto* request = mods::arg<node_create_request*>(args, 0);
    int result = cPhs_INIT_e;
    const int maxPasses = accelerated() ? 8 : 1;

    for (int pass = 0; pass < maxPasses; ++pass) {
        const int beforePhase = request != nullptr ? request->phase_request.id : 0;
        process_node_class* beforeNode = request != nullptr ? request->node_proc.node : nullptr;
        const fpc_ProcID beforeCreating = request != nullptr ? request->creating_id : 0;
        if (!accelerated()) {
            result = NodeRequestExecute::g_orig(request);
            break;
        }

        const int phaseResult = fpcNdRq_DoPhase(request);
        switch (phaseResult) {
        case cPhs_COMPLEATE_e:
            result = cPhs_NEXT_e;
            break;
        case cPhs_ERROR_e:
        case cPhs_UNK3_e:
            result = cPhs_UNK3_e;
            break;
        case cPhs_INIT_e:
        case cPhs_LOADING_e:
            result = cPhs_INIT_e;
            break;
        default:
            result = phaseResult;
            break;
        }
        if (result != cPhs_INIT_e || request == nullptr) break;
        if (request->phase_request.id == beforePhase && request->node_proc.node == beforeNode &&
            request->creating_id == beforeCreating)
        {
            break;
        }
    }

    if (retval != nullptr) *static_cast<int*>(retval) = result;
}

void create_request_handler_replace(ModContext*, void*, void* retval, void*) {
    int result = 1;
    const int maxPasses = accelerated() ? 64 : 1;
    const int maxUnchanged = accelerated() ? 16 : 1;
    int unchangedPasses = 0;

    for (int pass = 0; pass < maxPasses && g_fpcCtTg_Queue.mSize > 0; ++pass) {
        const int beforeSize = g_fpcCtTg_Queue.mSize;
        result = CreateRequestHandler::g_orig();
        if (result == 0 || !accelerated()) break;
        if (g_fpcCtTg_Queue.mSize == beforeSize) {
            if (++unchangedPasses >= maxUnchanged) break;
        } else {
            unchangedPasses = 0;
        }
    }

    if (retval != nullptr) *static_cast<int*>(retval) = result;
}

HookAction scene_request_pre(ModContext*, void* args, void*, void*) {
    if (!accelerated()) return HOOK_CONTINUE;

    const s16 processName = mods::arg<s16>(args, 2);
    s16& fadeName = mods::arg_ref<s16>(args, 4);
    const u16 peekTime = mods::arg<u16>(args, 5);
    titleToFileSelect = processName == fpcNm_NAME_SCENE_e &&
                        fadeName == fpcNm_OVERLAP0_e && peekTime == 5 &&
                        fpcM_SearchByName(fpcNm_TITLE_e) != nullptr;

    if (gameplay_fast_transition(processName)) {
        // This is the central MFB Fast Loads behavior from dScnPly: all ordinary
        // gameplay area changes use OVERLAP0 instead of the stage's slower wipe.
        // Merely shortening JUTFader did not bypass the original wipe actor's
        // timers, which is why the first port did not actually feel like MFB.
        fadeName = fpcNm_OVERLAP0_e;
        transitionActive = true;
        transitionSawNextStage = dComIfGp_isEnableNextStage();
    }

    return HOOK_CONTINUE;
}

HookAction set_peektime_pre(ModContext*, void* args, void*, void*) {
    if (!accelerated() || protected_transition()) return HOOK_CONTINUE;
    u16& peekTime = mods::arg_ref<u16>(args, 1);
    if (peekTime <= 0x7FFF) peekTime = std::min<u16>(peekTime, 1);
    return HOOK_CONTINUE;
}

HookAction fade_in_pre(ModContext*, void* args, void*, void*) {
    if (!transitionActive || !accelerated() || protected_transition()) return HOOK_CONTINUE;
    int& frames = mods::arg_ref<int>(args, 1);
    frames = std::min(frames, 25);
    return HOOK_CONTINUE;
}

HookAction fade_out_pre(ModContext*, void* args, void*, void*) {
    if (!transitionActive || !accelerated() || protected_transition()) return HOOK_CONTINUE;
    int& frames = mods::arg_ref<int>(args, 1);
    frames = std::min(frames, 20);
    return HOOK_CONTINUE;
}

void scene_phase_execute_post(ModContext*, void* args, void* retval, void*) {
    if (!accelerated() || protected_transition() || retval == nullptr) return;
    auto* request = mods::arg<scene_request_class*>(args, 0);
    const auto result = *static_cast<cPhs_Step*>(retval);
    if (request != nullptr && result == cPhs_NEXT_e && request->fade_request != nullptr &&
        request->create_request.name == fpcNm_PLAY_SCENE_e &&
        request->create_request.node_proc.node != nullptr &&
        fpcM_GetName(request->create_request.node_proc.node) == fpcNm_PLAY_SCENE_e)
    {
        // MFB releases the overlap as soon as the replacement play scene is ready.
        fopOvlpM_ClearOfReq();
    }
}

void overlap_fade_in_post(ModContext*, void* args, void*, void*) {
    if (!transitionActive || !accelerated() || protected_transition()) return;
    auto* overlap = mods::arg<overlap1_class*>(args, 0);
    if (overlap == nullptr) return;

    // Vanilla stores its independent 30-frame actor timer after starting the
    // fader. Match MFB's 20-frame timer and begin scene creation at frame 1.
    if (overlap->field_0xd0 > 19) overlap->field_0xd0 = 19;
    overlap->field_0xd4 = 20;
    if (overlap->field_0xd0 == 1) fopOvlpM_Done(overlap);
}

HookAction overlap_fade_out_pre(ModContext*, void* args, void*, void*) {
    if (!transitionActive || !accelerated() || protected_transition()) return HOOK_CONTINUE;
    auto* overlap = mods::arg<overlap1_class*>(args, 0);
    if (overlap != nullptr && overlap->field_0xcc == 0) {
        // MFB's incoming fade is 25 frames, independent of the outgoing timer.
        overlap->field_0xd4 = 25;
    }
    return HOOK_CONTINUE;
}

void overlap_fade_out_post(ModContext*, void* args, void*, void*) {
    if (!transitionActive || !accelerated() || protected_transition()) return;
    auto* overlap = mods::arg<overlap1_class*>(args, 0);
    if (overlap != nullptr && overlap->field_0xcc > 24) overlap->field_0xcc = 24;
}

void uninstall_all() {
    mods::hook::uninstall<OverlapFadeOut>();
    mods::hook::uninstall<OverlapFadeIn>();
    mods::hook::uninstall<ScenePhaseExecute>();
    mods::hook::uninstall<FaderStartOut>();
    mods::hook::uninstall<FaderStartIn>();
    mods::hook::uninstall<SetPeektime>();
    mods::hook::uninstall<SceneRequest>();
    mods::hook::uninstall<CreateRequestHandler>();
    mods::hook::uninstall<NodeRequestExecute>();
    mods::hook::uninstall<BaseSubCreate>();
    mods::hook::uninstall<PhaseDo>();
}
}  // namespace

ModResult install_hooks() {
    ModResult result = mods::hook::replace<PhaseDo>(phase_do_replace);
    if (result == MOD_OK) result = mods::hook::replace<BaseSubCreate>(base_sub_create_replace);
    if (result == MOD_OK) result = mods::hook::replace<NodeRequestExecute>(node_request_execute_replace);
    if (result == MOD_OK) result = mods::hook::replace<CreateRequestHandler>(create_request_handler_replace);
    if (result == MOD_OK) result = mods::hook::add_pre<SceneRequest>(scene_request_pre);
    if (result == MOD_OK) result = mods::hook::add_pre<SetPeektime>(set_peektime_pre);
    if (result == MOD_OK) result = mods::hook::add_pre<FaderStartIn>(fade_in_pre);
    if (result == MOD_OK) result = mods::hook::add_pre<FaderStartOut>(fade_out_pre);
    // These private vanilla functions provide the remaining MFB overlap timing.
    // Keep them optional so a symbol-manifest variation cannot disable the mod.
    if (result == MOD_OK) {
        mods::hook::add_post<ScenePhaseExecute>(scene_phase_execute_post);
        mods::hook::add_post<OverlapFadeIn>(overlap_fade_in_post);
        mods::hook::add_pre<OverlapFadeOut>(overlap_fade_out_pre);
        mods::hook::add_post<OverlapFadeOut>(overlap_fade_out_post);
    }
    if (result != MOD_OK) uninstall_all();
    return result;
}

void uninstall_hooks() {
    uninstall_all();
    transitionActive = false;
    transitionSawNextStage = false;
    titleToFileSelect = false;
}

void update() {
    if (!accelerated()) {
        transitionActive = false;
        transitionSawNextStage = false;
        titleToFileSelect = false;
        return;
    }


    if (transitionActive && dComIfGp_isEnableNextStage()) transitionSawNextStage = true;
    if (transitionActive && transitionSawNextStage && !dComIfGp_isEnableNextStage()) {
        transitionActive = false;
        transitionSawNextStage = false;
        titleToFileSelect = false;
    }
}
}  // namespace twilight_visuals::load_acceleration
