#include "running.hpp"
#include "service_refs.hpp"
#include "runtime.hpp"
#include "d/actor/d_a_alink.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_m3d.h"
#include "mods/service.hpp"
#include "hook_api.hpp"
#include "mods/svc/log.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "run_hold.hpp"
#include <algorithm>
#include <cstdio>
namespace twilight_visuals::running {
namespace {
RunHold aButton;
daAlink_c* inputPlayer = nullptr;
bool inputSampled = false;
s16 pendingRollAngle = 0;
daAlink_c* rollChainPlayer = nullptr;
bool previousUpdateStartedRolling = false;
bool justFinishedRoll = false;
bool humanSensesOwned = false;
unsigned transformTraceFrames = 0;
bool* humanWarpRequest = nullptr;
bool player_ready(daAlink_c* p) {
    return p != nullptr &&
        p == static_cast<daAlink_c*>(dComIfGp_getPlayer(0)) &&
        dComIfGp_getAttention() != nullptr;
}
bool enabled(daAlink_c* p) {
    if (!player_ready(p)) return false;

    // Targeting owns A's normal roll/evade actions, even without a locked actor.
    // Also respect switch-targeting, where lock-on outlasts the physical press.
    // Read the live attention object instead of Link's cached pointer. The execute
    // hook can run while Link is being created or destroyed, before that member is
    // initialized (or after it has become stale).
    return active() && runtime_settings().skywardSwordRunning &&
        !p->checkWolf() && !p->checkEventRun() &&
        !mDoCPd_c::getHoldL(PAD_1) && !dComIfGp_getAttention()->Lockon();
}
bool held(daAlink_c* p) {
    return enabled(p) && inputPlayer == p && aButton.running() && p->doButton();
}
bool moving(daAlink_c* p) { return held(p) && p->mProcID == daAlink_c::PROC_MOVE && p->mStickValue > 0.1f; }
f32 run_speed(daAlink_c* p) {
    return 37.0f * (p->checkEquipHeavyBoots() ? 0.70f : 1.0f);
}
bool water(daAlink_c* p) {
    if (!held(p) || !p->checkMagicArmorWearAbility() || p->checkMagneBootsOn() || p->mWaterY == -G_CM3D_F_INF) return false;
    const f32 offset = p->mWaterY - p->current.pos.y;
    return offset > -120.0f && offset < 180.0f;
}
bool grounded(daAlink_c* p) { return (p->mLinkAcch.ChkGroundHit() || water(p)) && !p->checkModeFlg(daAlink_c::MODE_SWIMMING); }
bool snow(daAlink_c* p) { return held(p) && p->checkSnowCode() && !p->checkBootsOrArmorHeavy(); }
DEFINE_HOOK(&daAlink_c::procFrontRollInit, RollInit);
DEFINE_HOOK(&daAlink_c::procFrontRoll, RollUpdate);
DEFINE_HOOK(&daAlink_c::procMove, Move);
DEFINE_HOOK(&daAlink_c::execute, PlayerExecute);
DEFINE_HOOK(&daAlink_c::checkMoveDoAction, MoveAction);
DEFINE_HOOK(&daAlink_c::checkFrontWallTypeAction, FrontWallAction);
DEFINE_HOOK(&daAlink_c::checkNormalAction, NormalAction);
DEFINE_HOOK(&daAlink_c::checkSideRollAction, SideRollAction);
DEFINE_HOOK(&daAlink_c::setStickData, StickData);
DEFINE_HOOK(&daAlink_c::procAutoJumpInit, AutoJumpInit);
DEFINE_HOOK(&dMeter2Draw_c::getActionString, ActionString);
DEFINE_HOOK(&daAlink_c::procStepMove, StepMove);
DEFINE_HOOK(&daAlink_c::setSandShapeOffset, SandSink);
DEFINE_HOOK(&daAlink_c::setBlendMoveAnime, MoveAnimation);
DEFINE_HOOK(&daAlink_c::setDoubleAnime, DoubleAnimation);
DEFINE_HOOK(&daAlink_c::checkSlope, AnimationSlope);
DEFINE_HOOK(&daAlink_c::procCoMetamorphoseInit, TransformInit);
unsigned moveAnimationDepth = 0;
daAlink_c* sprintRollPlayer = nullptr;
f32 sprintRollSpeed = 0.0f;
daAlink_c* sprintSheathPlayer = nullptr;
bool sprintSheathHandled = false;

HookAction front_wall_action_pre(ModContext*, void* args, void* retval, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (p != nullptr && held(p)) p->setFrontWallType();
    // checkFrontWallTypeAction increments this timer before the old MFB event
    // fired, so include the pending native increment in the threshold test.
    if (p != nullptr && moving(p) && p->mLinkAcch.ChkWallHit() &&
        !p->checkMagneBootsOn() && !p->checkModeFlg(daAlink_c::MODE_SWIMMING) &&
        (p->field_0x2f91 == 7 || p->field_0x2f91 == 8 || p->field_0x2f91 == 9) &&
        p->field_0x3078 + 1 > p->mpHIO->mWallHang.m.grab_input_time) {
        *static_cast<BOOL*>(retval) = p->procStepMoveInit();
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction normal_action_pre(ModContext*, void* args, void* retval, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!moving(p) || !grounded(p) || p->checkMagneBootsOn()) return HOOK_CONTINUE;
    if (p->spActionTrigger()) {
        *static_cast<int*>(retval) = p->procFrontRollInit();
        return HOOK_SKIP_ORIGINAL;
    }
    if (p->swordTrigger()) {
        if (p->mEquipItem == 0x103 && !p->checkEquipAnime()) {
            *static_cast<int*>(retval) = p->procCutJumpInit(FALSE);
            return HOOK_SKIP_ORIGINAL;
        }
        if (p->mEquipItem != 0x103 && p->checkSwordGet() && !p->checkEquipAnime() &&
            !p->checkNotBattleStage() &&
            (!p->checkModeFlg(0x40000) || p->checkEquipHeavyBoots())) {
            p->swordEquip(TRUE);
            *static_cast<int*>(retval) = 1;
            return HOOK_SKIP_ORIGINAL;
        }
    }
    return HOOK_CONTINUE;
}

HookAction side_roll_action_pre(ModContext*, void* args, void* retval, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!held(p)) return HOOK_CONTINUE;
    *static_cast<BOOL*>(retval) = FALSE;
    return HOOK_SKIP_ORIGINAL;
}

void stick_data_post(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!snow(p)) return;
    if (p->mHeavySpeedMultiplier > 0.0001f)
        p->mStickValue = std::clamp(p->mStickValue / p->mHeavySpeedMultiplier, 0.0f, 1.0f);
    p->mHeavySpeedMultiplier = 1.0f;
}

HookAction auto_jump_init_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (moving(p) && !p->checkMagneBootsOn()) mods::arg_ref<int>(args, 1) = 2;
    return HOOK_CONTINUE;
}

void auto_jump_init_post(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (p != nullptr && mods::arg<int>(args, 1) == 2 && enabled(p)) {
        p->mNormalSpeed *= 1.50f;
        p->speedF = p->mNormalSpeed;
    }
}

void apply_run_speed(daAlink_c* p) {
    if (p != nullptr && moving(p) && grounded(p)) {
        if (sprintSheathPlayer != p) {
            sprintSheathPlayer = p;
            sprintSheathHandled = false;
        }
        if (runtime_settings().sheathSwordWhileSprinting && !sprintSheathHandled) {
            if (p->mEquipItem != 0x103) {
                sprintSheathHandled = true;
            } else if (!p->checkEquipAnime()) {
                p->swordUnequip();
                sprintSheathHandled = true;
            }
        }
        p->mNormalSpeed = run_speed(p);
        p->speedF = p->mNormalSpeed;
    } else if (p == sprintSheathPlayer) {
        sprintSheathPlayer = nullptr;
        sprintSheathHandled = false;
    }
}

void move_post(ModContext*, void* args, void*, void*) {
    apply_run_speed(mods::arg<daAlink_c*>(args, 0));
}

void trace_transform(daAlink_c* p, const char* point, int result) {
    if (!svc_log) return;
    char message[768];
    const auto* event = dComIfGp_getEvent();
    std::snprintf(message, sizeof(message),
        "TransformTrace %s result=%d proc=%d wolf=%d clothesTimer=%d phase=%d finished=%d wait=%d "
        "anim=%.2f demoMode=%d demoType=%d event=%d compulsory=%d map=%d nextStage=%d "
        "humanWarp=%d ground=%d",
        point, result, static_cast<int>(p->mProcID), !!p->checkWolf(),
        static_cast<int>(p->mClothesChangeWaitTimer),
        static_cast<int>(p->mProcVar0.field_0x3008),
        static_cast<int>(p->mProcVar5.field_0x3012),
        static_cast<int>(p->mProcVar1.field_0x300a),
        static_cast<double>(p->mUnderFrameCtrl[0].getFrame()),
        static_cast<int>(p->mDemo.getDemoMode()), static_cast<int>(p->mDemo.getDemoType()),
        !!p->checkEventRun(), static_cast<int>(dComIfGp_getEvent()->checkCompulsory()),
        static_cast<int>(dMeter2Info_getMapStatus()), !!dComIfGp_isEnableNextStage(),
        humanWarpRequest ? static_cast<int>(*humanWarpRequest) : -1,
        !!p->mLinkAcch.ChkGroundHit());
    svc_log->info(mod_ctx, message);
}
HookAction transform_init_pre(ModContext*, void* args, void*, void*) {
    transformTraceFrames = 0;
    auto* p = mods::arg<daAlink_c*>(args, 0);
    trace_transform(p, "init-enter", -1);
    // The reproduced map-glitch softlock enters from idle, with no event,
    // but still has the Warp as Human request latched. A genuine warp's
    // scripted transformation is already in an event and must retain it.
    // Do not require map=0: the glitch specifically leaves map=1 behind.
    if (active() && humanWarpRequest && *humanWarpRequest &&
        !p->checkWolf() && !p->checkEventRun() &&
        !dComIfGp_isEnableNextStage() &&
        (p->mProcID == daAlink_c::PROC_WAIT || p->mProcID == daAlink_c::PROC_MOVE)) {
        *humanWarpRequest = false;
        if (svc_log) svc_log->info(mod_ctx,
            "TransformTrace cleared stale human-warp request for voluntary transformation.");
    }
    return HOOK_CONTINUE;
}
void transform_init_post(ModContext*, void* args, void* retval, void*) {
    trace_transform(mods::arg<daAlink_c*>(args, 0), "init-return", *static_cast<int*>(retval));
}

HookAction input_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!player_ready(p)) {
        aButton = {};
        inputPlayer = nullptr;
        rollChainPlayer = nullptr;
        previousUpdateStartedRolling = false;
        justFinishedRoll = false;
        inputSampled = false;
        transformTraceFrames = 0;
        humanSensesOwned = false;
        return HOOK_CONTINUE;
    }
    const bool humanSensesEnabled = active() && runtime_settings().humanWolfSenses;
    if (humanSensesOwned && (!humanSensesEnabled || p->checkWolf())) {
        if (!p->checkWolf()) p->offWolfEyeUp();
        humanSensesOwned = false;
    }
    if (humanSensesEnabled && !p->checkWolf() && !p->checkEventRun() &&
        dComIfGs_isEventBit(dSv_event_flag_c::F_0550) && mDoCPd_c::getTrigDown(PAD_1)) {
        if (p->checkWolfEyeUp()) {
            p->offWolfEyeUp();
            humanSensesOwned = false;
        } else {
            p->onWolfEyeUp();
            humanSensesOwned = true;
        }
        mDoCPd_c::getCpadInfo(PAD_1).mPressedButtonFlags &= ~PAD_BUTTON_DOWN;
    }
    if (p->mProcID == daAlink_c::PROC_METAMORPHOSE ||
        p->mProcID == daAlink_c::PROC_METAMORPHOSE_ONLY) {
        if (transformTraceFrames < 900 && transformTraceFrames++ % 30 == 0)
            trace_transform(p, "progress", -1);
    } else if (transformTraceFrames) {
        trace_transform(p, "exit", -1);
        transformTraceFrames = 0;
    }
    if (!enabled(p)) aButton = {};
    const bool rolling = p->mProcID == daAlink_c::PROC_FRONT_ROLL;
    justFinishedRoll = rollChainPlayer == p && previousUpdateStartedRolling && !rolling;
    rollChainPlayer = p;
    previousUpdateStartedRolling = rolling;
    inputSampled = false;
    return HOOK_CONTINUE;
}

HookAction move_action_pre(ModContext*, void* args, void* retval, void*) {
    if (inputSampled) return HOOK_CONTINUE;
    inputSampled = true;
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (inputPlayer != p) {
        aButton = {};
        inputPlayer = p;
    }
    const bool normalMovement = p->mProcID == daAlink_c::PROC_MOVE ||
        p->mProcID == daAlink_c::PROC_WAIT;
    const auto status = dComIfGp_getDoStatus();

    // The MFB host used to inject this decision into the player event bus.
    // Recreate it at the native action boundary: while sprinting into a
    // climbable wall, enter the existing step-move state before vanilla
    // consumes the action as a regular roll/attack.
    // Below the native roll-stick threshold the HUD can have no action.
    // A deliberate directional A press should still roll, without charging.
    if (enabled(p) && normalMovement && p->doTrigger() && p->checkInputOnR() &&
        p->mStickValue <= p->getFrontRollRate() &&
        (status == BUTTON_STATUS_NONE || status == BUTTON_STATUS_UNK_121) &&
        grounded(p) && !p->checkMagneBootsOn() && !p->checkNotJumpSinkLimit()) {
        aButton = {};
        p->shape_angle.y = p->mMoveAngle;
        *static_cast<BOOL*>(retval) = p->procFrontRollInit();
        return HOOK_SKIP_ORIGINAL;
    }
    // A new roll-chain press must reach vanilla on its trigger frame, not
    // enter Ready and wait for release. Vanilla still enforces cancel timing.
    if (enabled(p) && dComIfGp_getDoStatus() == BUTTON_STATUS_UNK_121 &&
        nativeRollChainPress(p->doTrigger(),
            p->mProcID == daAlink_c::PROC_FRONT_ROLL,
            justFinishedRoll && normalMovement)) {
        aButton = {};
        return HOOK_CONTINUE;
    }
    // Mirror procWolfRollAttackCharge's release-before-charge decision, using
    // its ready interpolation duration without changing Link's animation.
    // This hook runs after the game builds mItemButton/mItemTrigger.
    const bool eligible = enabled(p) &&
        ((normalMovement && dComIfGp_getDoStatus() == BUTTON_STATUS_UNK_121) ||
         (normalMovement && aButton.state == RunHold::State::Ready &&
             status == BUTTON_STATUS_NONE) ||
         (aButton.running() && (normalMovement ||
             p->mProcID == daAlink_c::PROC_FRONT_ROLL ||
             p->mProcID == daAlink_c::PROC_STEP_MOVE)));
    if (eligible && aButton.state == RunHold::State::Idle && p->doTrigger())
        pendingRollAngle = p->checkInputOnR() ? p->mMoveAngle : p->shape_angle.y;
    if (aButton.update(p->doTrigger(), p->doButton(), eligible,
                       p->mpHIO->mWolf.mWlAttack.m.mReadyInterpolation)) {
        if (grounded(p) && !p->checkMagneBootsOn()) {
            p->shape_angle.y = p->checkInputOnR() ? p->mMoveAngle : pendingRollAngle;
            *static_cast<BOOL*>(retval) = p->procFrontRollInit();
            return HOOK_SKIP_ORIGINAL;
        }
    }
    apply_run_speed(p);
    if (eligible && aButton.state != RunHold::State::Idle) {
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction move_animation_pre(ModContext*, void*, void*, void*) {
    ++moveAnimationDepth;
    return HOOK_CONTINUE;
}
void move_animation_post(ModContext*, void*, void*, void*) {
    if (moveAnimationDepth) --moveAnimationDepth;
}
HookAction double_animation_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!moveAnimationDepth || !moving(p)) return HOOK_CONTINUE;

    // The MFB host changed setBlendMoveAnime's local animation IDs before
    // its native setDoubleAnime call. Do the same at that call boundary so
    // the game's walk/run blend, frame continuity and morph timing survive.
    auto& animationA = mods::arg_ref<daAlink_c::daAlink_ANM>(args, 4);
    auto& animationB = mods::arg_ref<daAlink_c::daAlink_ANM>(args, 5);
    auto& rateB = mods::arg_ref<f32>(args, 3);
    if (animationA == daAlink_c::ANM_RUN) animationA = daAlink_c::ANM_RUN_B;
    if (animationB == daAlink_c::ANM_RUN) animationB = daAlink_c::ANM_RUN_B;
    if (p->checkEquipHeavyBoots() && grounded(p)) {
        // This is the MFB HeavyRun override of var_r28 and sp2C.
        animationA = daAlink_c::ANM_RUN_B;
        rateB *= 0.70f;
    }
    return HOOK_CONTINUE;
}
HookAction animation_slope_pre(ModContext*, void* args, void* retval, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    // Suppress only the slope-walk animation branch, not terrain physics.
    if (moveAnimationDepth && moving(p) && grounded(p)) {
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction sand_sink_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    // Only actual SS running on sand bypasses sinking. Snow, other forms,
    // ordinary movement and released A retain the native sink calculation.
    if (!moving(p) || !p->mLinkAcch.ChkGroundHit() ||
        p->mGndPolyAtt0 != 3 || p->checkSnowCode()) return HOOK_CONTINUE;
    p->mSinkShapeOffset = 0.0f;
    p->field_0x2fc9 = 0x10;
    p->mZ2Link.setSinkDepth(-1);
    return HOOK_SKIP_ORIGINAL;
}

void step_move_post(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (inputPlayer != p) return;
    // Observe release even before the climb's normal action/cancel window.
    if (!enabled(p) || !p->doButton()) {
        aButton = {};
        return;
    }
    // Leave the climb animation and placement alone; resume speed on exit.
    if (moving(p) && grounded(p)) {
        p->mNormalSpeed = run_speed(p);
    }
}

void action_string_post(ModContext*, void* args, void* retval, void*) {
    // Display-only override: leave the native Roll status intact for gameplay.
    if (mods::arg<u8>(args, 1) != BUTTON_STATUS_UNK_121) return;
    auto* p = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (!p || !enabled(p)) return;
    if (p->mProcID != daAlink_c::PROC_MOVE && p->mProcID != daAlink_c::PROC_WAIT) return;
    static char runText[] = "Run";
    *static_cast<char**>(retval) = runText;
}

HookAction roll_pre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    // Capture before vanilla initialization caps the roll's entry speed.
    sprintRollPlayer = moving(p) && grounded(p) ? p : nullptr;
    sprintRollSpeed = sprintRollPlayer ? p->mNormalSpeed : 0.0f;
    return HOOK_CONTINUE;
}

void roll_post(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (p != sprintRollPlayer) return;
    // Never carry momentum into a crash, another action, or a disabled mod.
    if (!enabled(p) || p->mProcID != daAlink_c::PROC_FRONT_ROLL ||
        !p->mLinkAcch.ChkGroundHit() || p->mLinkAcch.ChkWallHit()) {
        sprintRollPlayer = nullptr;
        return;
    }
    cM3dGPla poly;
    if (p->getSlidePolygon(&poly)) {
        sprintRollPlayer = nullptr;
        return;
    }
    p->mNormalSpeed = sprintRollSpeed;
}
}
void initialize() {
    // This host data symbol is not directly exported; use the mod resolver.
    void* warpAddress = nullptr;
    HookSymbolFlags warpFlags{};
    if (twilight_hook_service()->resolve(mod_ctx, "?sDuskHumanWarpRequest@daAlink_c@@2_NA",
                          &warpAddress, &warpFlags) == MOD_OK &&
        (warpFlags & HOOK_SYMBOL_DATA)) {
        humanWarpRequest = static_cast<bool*>(warpAddress);
    } else {
        if (svc_log) svc_log->warn(mod_ctx, "TransformTrace: human-warp flag unavailable; other diagnostics remain active.");
    }
    const auto tracePre = mods::hook::add_pre<TransformInit>(transform_init_pre);
    const auto tracePost = mods::hook::add_post<TransformInit>(transform_init_post);
    if (svc_log) svc_log->info(mod_ctx, tracePre == MOD_OK && tracePost == MOD_OK ?
        "TransformTrace enabled; guarded voluntary-transformation warp-flag fix active." :
        "TransformTrace initialization hook unavailable.");
    mods::hook::add_pre<MoveAnimation>(move_animation_pre);
    mods::hook::add_post<MoveAnimation>(move_animation_post);
    mods::hook::add_pre<DoubleAnimation>(double_animation_pre);
    mods::hook::add_pre<AnimationSlope>(animation_slope_pre);
    mods::hook::add_pre<SandSink>(sand_sink_pre);
    mods::hook::add_post<StepMove>(step_move_post);
    mods::hook::add_post<ActionString>(action_string_post);
    mods::hook::add_pre<MoveAction>(move_action_pre);
    mods::hook::add_pre<FrontWallAction>(front_wall_action_pre);
    mods::hook::add_pre<NormalAction>(normal_action_pre);
    mods::hook::add_pre<SideRollAction>(side_roll_action_pre);
    mods::hook::add_post<StickData>(stick_data_post);
    mods::hook::add_pre<AutoJumpInit>(auto_jump_init_pre);
    mods::hook::add_post<AutoJumpInit>(auto_jump_init_post);
    mods::hook::add_pre<PlayerExecute>(input_pre);
    mods::hook::add_pre<RollInit>(roll_pre);
    mods::hook::add_post<RollInit>(roll_post);
    mods::hook::add_post<RollUpdate>(roll_post);
    mods::hook::add_post<Move>(move_post);
}
bool is_running() {
    auto* p = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    return p && moving(p) && grounded(p);
}
void refresh_run_speed(daAlink_c* player) { apply_run_speed(player); }
void shutdown() {
    humanSensesOwned = false;
    humanWarpRequest = nullptr;
    transformTraceFrames = 0;
    mods::hook::uninstall<TransformInit>();
    mods::hook::uninstall<AnimationSlope>();
    mods::hook::uninstall<MoveAnimation>();
    mods::hook::uninstall<DoubleAnimation>();
    moveAnimationDepth = 0;
    mods::hook::uninstall<SandSink>();
    mods::hook::uninstall<StepMove>();
    mods::hook::uninstall<ActionString>();
    mods::hook::uninstall<AutoJumpInit>();
    mods::hook::uninstall<StickData>();
    mods::hook::uninstall<SideRollAction>();
    mods::hook::uninstall<NormalAction>();
    mods::hook::uninstall<FrontWallAction>();
    rollChainPlayer = nullptr;
    previousUpdateStartedRolling = false;
    justFinishedRoll = false;
    mods::hook::uninstall<MoveAction>();
    aButton = {};
    inputPlayer = nullptr;
    mods::hook::uninstall<PlayerExecute>();
    sprintRollPlayer = nullptr;
    sprintSheathPlayer = nullptr;
    sprintSheathHandled = false;
    mods::hook::uninstall<RollUpdate>();
    mods::hook::uninstall<RollInit>();
    mods::hook::uninstall<Move>();
}
}
