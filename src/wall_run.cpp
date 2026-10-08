#include "wall_run.hpp"

#include "runtime.hpp"
#include "running.hpp"
#include "platform.hpp"
#include "external_assets.hpp"
#include "service_refs.hpp"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "mods/service.hpp"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"
#include "JSystem/J3DGraphLoader/J3DAnmLoader.h"

#include "animation_validation.h"

#include <cmath>
#include <filesystem>
#include <vector>

namespace twilight_visuals::wall_run {
namespace {

DEFINE_HOOK(&daAlink_c::procMove, Move);
DEFINE_HOOK(&daAlink_c::procWait, Wait);
DEFINE_HOOK(&daAlink_c::procAutoJump, Jump);
DEFINE_HOOK(&daAlink_c::execute, Execute);
DEFINE_HOOK(&daAlink_c::procHangWallCatchInit, HangWallCatchInit);
DEFINE_HOOK(&daAlink_c::procHangWallCatch, HangWallCatch);

struct Clip {
    ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
    mDoExt_transAnmBas animation{nullptr};
    std::vector<unsigned char> externalData;
    bool external = false;
    bool ready = false;
};

Clip wallClip;
Clip ledgeGrabClip;
daAlink_c* runner = nullptr;
int remaining = 0;
float startY = 0.0f;
float savedGravity = 0.0f;
s16 wallAngle = 0;
cXyz ledgePos;
u8 ledgeType = 0;
bool assistedLedge = false;
bool requireRelease = false;
bool shortRun = false;
cXyz shortTop;
bool ledgeGrabPending = false;
int shortWallRecovery = 0;
int diagnosticDelay = 0;
bool installed = false;

constexpr float riseSpeed = 16.0f;
constexpr float maxRise = 256.0f;
constexpr float assistedGrabHeight = 35.0f;
constexpr float shortWallHeight = 180.0f;
constexpr float shortWallForward = 24.0f;
constexpr int shortWallStrideTicks = 8;
constexpr int shortWallRecoveryTicks = 10;

bool enabled(daAlink_c* p) {
    return p != nullptr && p == static_cast<daAlink_c*>(dComIfGp_getLinkPlayer()) &&
        active() && runtime_settings().skywardSwordRunning &&
        runtime_settings().skywardSwordWallRunning;
}

bool inputSafe(daAlink_c* p) {
    return p->mStickValue > 0.7f && !p->checkWolf() &&
        !p->checkEventRun() && !p->checkAttentionLock() &&
        !p->checkBootsOrArmorHeavy();
}

bool ground(daAlink_c* p) {
    return (p->mProcID == daAlink_c::PROC_MOVE || p->mProcID == daAlink_c::PROC_WAIT) &&
        p->mLinkAcch.ChkGroundHit();
}

bool intent(daAlink_c* p) {
    return enabled(p) && running::is_running() && inputSafe(p) && ground(p);
}

bool loadClip(Clip& clip, const char* path) {
    if (svc_resource->load(mod_ctx, path, &clip.buffer) != MOD_OK) {
        const std::string message =
            "Twilight Visuals: bundled animation resource could not be loaded: " +
            std::string(path);
        svc_log->error(mod_ctx, message.c_str());
        return false;
    }
    const auto* data = static_cast<const unsigned char*>(clip.buffer.data);
    if (!ss::validBck(data, clip.buffer.size)) {
        const std::string message =
            "Twilight Visuals: bundled animation resource is not a valid BCK file: " +
            std::string(path);
        svc_log->error(mod_ctx, message.c_str());
        svc_resource->free(mod_ctx, &clip.buffer);
        clip.buffer = RESOURCE_BUFFER_INIT;
        return false;
    }
    J3DAnmLoaderDataBase::setResource(&clip.animation, data);
    clip.ready = true;
    return true;
}

void logExternalClipError(const std::filesystem::path& path, const std::string& reason) {
    const std::string message = "Twilight Visuals: " + reason + "\nExpected animation file: " +
        external_assets::display_path(path) +
        "\nRun the SS-TP Animation Converter, copy its output into the custom "
        "assets/animations folder, and restart Dusklight.";
    svc_log->error(mod_ctx, message.c_str());
}

bool loadExternalClip(Clip& clip, const char* filename) {
    const auto adjacentDirectory = platform::custom_animation_directory();
    if (adjacentDirectory.empty()) {
        const std::string message =
            "Twilight Visuals: could not determine the platform custom asset directory, so "
            "the animations folder cannot be located. Required file: " +
            std::string(filename);
        svc_log->error(mod_ctx, message.c_str());
        return false;
    }

    const auto path = adjacentDirectory / filename;
    const auto directoryStatus = external_assets::ensure_directory(adjacentDirectory);
    if (!directoryStatus) {
        logExternalClipError(path, "could not prepare the custom assets/animations folder (" +
            directoryStatus.reason + ").");
        return false;
    }

    const auto fileStatus = external_assets::read_binary(path, clip.externalData);
    if (!fileStatus) {
        logExternalClipError(path, std::string("could not load ") + filename + " (" +
            fileStatus.reason + ").");
        return false;
    }
    if (!ss::validBck(clip.externalData.data(), clip.externalData.size())) {
        clip.externalData.clear();
        logExternalClipError(path,
            std::string("required animation file is not a valid BCK: ") + filename);
        return false;
    }
    clip.buffer.data = clip.externalData.data();
    clip.buffer.size = clip.externalData.size();
    J3DAnmLoaderDataBase::setResource(&clip.animation, clip.buffer.data);
    clip.external = true;
    clip.ready = true;
    const std::string message = "Twilight Visuals: loaded custom animation: " +
        external_assets::display_path(path);
    svc_log->info(mod_ctx, message.c_str());
    return true;
}

void freeClips() {
    wallClip.ready = false;
    ledgeGrabClip.ready = false;
    for (Clip* clip : {&wallClip, &ledgeGrabClip}) {
        if (!clip->external) svc_resource->free(mod_ctx, &clip->buffer);
        clip->externalData.clear();
        clip->buffer = RESOURCE_BUFFER_INIT;
        clip->external = false;
    }
}

bool customAnimation(daAlink_c* p) {
    for (int i = 0; i < 2; ++i) {
        auto* under = p->mNowAnmPackUnder[i].getAnmTransform();
        auto* upper = p->mNowAnmPackUpper[i].getAnmTransform();
        if (under == &wallClip.animation || under == &ledgeGrabClip.animation ||
            upper == &wallClip.animation || upper == &ledgeGrabClip.animation) {
            return true;
        }
    }
    return false;
}

void playClip(daAlink_c* p, Clip& clip, float rate) {
    p->setSingleAnimeBaseSpeed(daAlink_c::ANM_RUN_B, 2.0f, 3.0f);
    auto* nativeUnder = p->mNowAnmPackUnder[0].getAnmTransform();
    auto* nativeUpper = p->mNowAnmPackUpper[0].getAnmTransform();
    p->commonSingleAnime(&clip.animation, nativeUpper != nativeUnder ? nativeUpper : nullptr,
        rate, 0.0f, -1);
    p->resetBasAnime();
}

void detachClip(daAlink_c* p) {
    if (!customAnimation(p)) return;
    p->mUnderAnmHeap[0].resetIdx();
    p->mUpperAnmHeap[0].resetIdx();
    p->setSingleAnimeBase(daAlink_c::ANM_WAIT);
}

void playLedgeGrabClip(daAlink_c* p) {
    // The SS catch is a coordinated full-body transition. Mixing TP's native
    // upper track into it twists the torso and arms away from the retargeted pose.
    p->commonSingleAnime(&ledgeGrabClip.animation, nullptr, 1.0f, 2.0f, -1);
    p->resetBasAnime();
}

void applyPendingLedgeGrab(daAlink_c* p) {
    if (!ledgeGrabPending || !enabled(p) || !ledgeGrabClip.ready ||
        p->mProcID != daAlink_c::PROC_HANG_WALL_CATCH ||
        p->getNowAnmPackUnder(daAlink_c::UNDER_0) == &ledgeGrabClip.animation) {
        return;
    }
    playLedgeGrabClip(p);
    ledgeGrabPending = false;
    svc_log->debug(mod_ctx, "Playing SS airborne ledge catch; native hang state retained.");
}

void hangWallCatchInitPost(ModContext*, void* args, void*, void*) {
    applyPendingLedgeGrab(mods::arg<daAlink_c*>(args, 0));
}

HookAction hangWallCatchPre(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    applyPendingLedgeGrab(p);
    return HOOK_CONTINUE;
}

bool wall(daAlink_c* p, s16 angle) {
    const float radians = angle * (3.14159265358979323846f / 32768.0f);
    cXyz from(p->current.pos.x, p->current.pos.y + 70.0f, p->current.pos.z);
    cXyz to(from.x + std::sin(radians) * 85.0f, from.y,
        from.z + std::cos(radians) * 85.0f);
    if (!p->commonLineCheck(&from, &to)) return false;
    cM3dGPla plane;
    dComIfG_Bgsp().GetTriPla(p->mLinkLinChk, &plane);
    return std::fabs(plane.mNormal.y) < 0.15f &&
        plane.mNormal.x * std::sin(radians) + plane.mNormal.z * std::cos(radians) < -0.9f;
}

bool findShortTop(daAlink_c* p, s16 angle, cXyz& top) {
    const float radians = angle * (3.14159265358979323846f / 32768.0f);
    const cXyz hit = p->mLinkLinChk.GetCross();
    constexpr float depths[] = {30.0f, 44.0f, 18.0f};
    for (float depth : depths) {
        cXyz probe(hit.x + std::sin(radians) * depth, startY + shortWallHeight + 40.0f,
            hit.z + std::cos(radians) * depth);
        p->mLinkGndChk.SetPos(&probe);
        const float floor = dComIfG_Bgsp().GroundCross(&p->mLinkGndChk);
        cM3dGPla plane;
        const float rise = floor - startY;
        if (floor != -G_CM3D_F_INF && rise >= 20.0f && rise <= shortWallHeight &&
            dComIfG_Bgsp().GetTriPla(p->mLinkGndChk, &plane) &&
            cBgW_CheckBGround(plane.mNormal.y)) {
            top.set(probe.x, floor, probe.z);
            return true;
        }
    }
    return false;
}

void stop(daAlink_c* p) {
    remaining = 0;
    runner = nullptr;
    shortRun = false;
    ledgeGrabPending = false;
    p->gravity = savedGravity;
    requireRelease = true;
    svc_log->info(mod_ctx, "Wall run ended; release A to rearm.");
}

bool finishShortRun(daAlink_c* p) {
    p->current.pos = shortTop;
    p->old.pos = shortTop;
    p->gravity = savedGravity;
    p->speed.y = 0.0f;
    p->mLinkAcch.SetGroundHit();
    remaining = 0;
    runner = nullptr;
    shortRun = false;
    ledgeGrabPending = false;
    ledgeType = 0;
    detachClip(p);
    p->procMoveInit();
    running::refresh_run_speed(p);
    const bool keepRunning = running::is_running();
    shortWallRecovery = keepRunning ? shortWallRecoveryTicks : 0;
    requireRelease = !keepRunning;
    svc_log->info(mod_ctx, keepRunning
        ? "Completed short-wall step-up; beginning grounded recovery stride."
        : "Completed one-stride short-wall step-up.");
    return true;
}

void scanLedge(daAlink_c* p) {
    p->field_0x2f91 = 0;
    p->setFrontWallType();
    if (p->field_0x2f91 != 10 && p->field_0x2f91 != 11) {
        const float y = p->current.pos.y;
        p->current.pos.y = y + assistedGrabHeight;
        p->field_0x2f91 = 0;
        p->setFrontWallType();
        p->current.pos.y = y;
        assistedLedge = p->field_0x2f91 == 10 || p->field_0x2f91 == 11;
    } else {
        assistedLedge = false;
    }
    if (p->field_0x2f91 == 10 || p->field_0x2f91 == 11) {
        ledgeType = p->field_0x2f91;
        ledgePos = p->field_0x34ec;
    }
}

bool tryRunOntoShortWall(daAlink_c* p) {
    const float radians = wallAngle * (3.14159265358979323846f / 32768.0f);
    if (!ledgeType || ledgePos.y - startY > shortWallHeight) return false;
    cXyz top(ledgePos.x + std::sin(radians) * shortWallForward, ledgePos.y + 90.0f,
        ledgePos.z + std::cos(radians) * shortWallForward);
    p->mLinkGndChk.SetPos(&top);
    const float floor = dComIfG_Bgsp().GroundCross(&p->mLinkGndChk);
    cM3dGPla plane;
    if (floor == -G_CM3D_F_INF || std::fabs(floor - ledgePos.y) > 35.0f ||
        !dComIfG_Bgsp().GetTriPla(p->mLinkGndChk, &plane) ||
        !cBgW_CheckBGround(plane.mNormal.y)) {
        return false;
    }
    p->current.pos.set(top.x, floor, top.z);
    p->old.pos = p->current.pos;
    p->gravity = savedGravity;
    p->speed.y = 0.0f;
    p->mLinkAcch.SetGroundHit();
    remaining = 0;
    runner = nullptr;
    ledgeType = 0;
    detachClip(p);
    p->procMoveInit();
    running::refresh_run_speed(p);
    const bool keepRunning = running::is_running();
    shortWallRecovery = keepRunning ? shortWallRecoveryTicks : 0;
    requireRelease = !keepRunning;
    svc_log->info(mod_ctx, "Completed short-wall run and preserved forward momentum.");
    return true;
}

void executePost(ModContext*, void* args, void*, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!p) return;
    if (ledgeGrabPending && p->mProcID != daAlink_c::PROC_HANG_WALL_CATCH &&
        p->mProcID != daAlink_c::PROC_AUTO_JUMP) {
        ledgeGrabPending = false;
    }
    if (!p->doButton()) requireRelease = false;
    if (shortWallRecovery > 0) {
        if (running::is_running() && p->doButton()) {
            --shortWallRecovery;
        } else {
            shortWallRecovery = 0;
        }
    }
    if (diagnosticDelay > 0) --diagnosticDelay;
    if (p == runner && (p->checkEventRun() || p->checkWolf() ||
        p->mProcID != daAlink_c::PROC_AUTO_JUMP)) {
        if (p->mProcID == daAlink_c::PROC_AUTO_JUMP) p->gravity = savedGravity;
        remaining = 0;
        runner = nullptr;
        shortRun = false;
        requireRelease = true;
        detachClip(p);
        svc_log->info(mod_ctx, "Wall run handed off to another player action.");
    }
}

HookAction movePre(ModContext*, void* args, void* result, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (!intent(p) || runner || requireRelease || shortWallRecovery > 0) {
        return HOOK_CONTINUE;
    }
    if (!wall(p, p->shape_angle.y)) {
        if (!diagnosticDelay) {
            svc_log->info(mod_ctx, "SS wall run enabled; no facing vertical wall within 85 units.");
            diagnosticDelay = 180;
        }
        return HOOK_CONTINUE;
    }
    savedGravity = p->gravity;
    startY = p->current.pos.y;
    wallAngle = p->shape_angle.y;
    shortRun = findShortTop(p, wallAngle, shortTop);
    if (!p->procAutoJumpInit(1)) {
        shortRun = false;
        return HOOK_CONTINUE;
    }
    runner = p;
    remaining = shortRun ? shortWallStrideTicks : 16;
    ledgeType = 0;
    assistedLedge = false;
    playClip(p, wallClip, 1.0f);
    svc_log->info(mod_ctx, shortRun
        ? "Short wall detected; committing one wall-run stride before step-up."
        : "Wall run started (retargeted SS dashUpL/R, one-shot playback).");
    p->mLinkAcch.ClrGroundHit();
    p->gravity = 0.0f;
    p->speed.y = riseSpeed;
    p->mNormalSpeed = 3.0f;
    *static_cast<int*>(result) = 1;
    return HOOK_SKIP_ORIGINAL;
}

HookAction jumpPre(ModContext*, void* args, void* result, void*) {
    auto* p = mods::arg<daAlink_c*>(args, 0);
    if (p != runner || !remaining) return HOOK_CONTINUE;
    if (p->mLinkAcch.ChkRoofHit()) {
        stop(p);
        p->procFallInit(1, 3.0f);
    } else {
        p->gravity = savedGravity;
        scanLedge(p);
        if (shortRun && --remaining <= 0) {
            finishShortRun(p);
        } else {
            const bool animationDone = !shortRun &&
                (p->checkAnmEnd(&p->mUnderFrameCtrl[0]) || --remaining == 0 ||
                 p->current.pos.y - startY >= maxRise);
            if (animationDone) {
                if (ledgeType) {
                    p->field_0x2f91 = ledgeType;
                    p->field_0x34ec = ledgePos;
                } else {
                    p->field_0x2f91 = 0;
                }
                if (tryRunOntoShortWall(p)) {
                    // The short-wall path has entered PROC_MOVE.
                } else {
                    int ledgeAction = 0;
                    if (ledgeType == 10) {
                        // checkFrontWallTypeAction() rescans from the new airborne
                        // position and loses assisted/cached ledges. Use the same
                        // native initializer directly after restoring its cached
                        // contact point.
                        p->field_0x34ec = ledgePos;
                        p->field_0x2f91 = ledgeType;
                        ledgeGrabPending = false;
                        ledgeAction = p->procHangStartInit();
                    } else if (ledgeType == 11) {
                        p->field_0x34ec = ledgePos;
                        p->field_0x2f91 = ledgeType;
                        ledgeGrabPending = true;
                        ledgeAction = p->procHangWallCatchInit();
                    } else {
                        ledgeAction = p->checkFrontWallTypeAction();
                        if (ledgeAction && p->mProcID == daAlink_c::PROC_HANG_WALL_CATCH) {
                            ledgeGrabPending = true;
                        }
                    }

                    if (ledgeAction) {
                        remaining = 0;
                        runner = nullptr;
                        requireRelease = true;
                        ledgeType = 0;
                        svc_log->info(mod_ctx, assistedLedge
                            ? "Completed wall animation and used assisted ledge grab."
                            : "Completed wall animation and transitioned to ledge grab.");
                    } else {
                        ledgeGrabPending = false;
                        stop(p);
                        p->procFallInit(1, 3.0f);
                    }
                }
            } else {
                p->gravity = 0.0f;
                const float riseLimit = shortRun ? shortTop.y - startY : maxRise;
                p->speed.y = std::fmax(0.0f,
                    std::fmin(riseSpeed, riseLimit - (p->current.pos.y - startY)));
                p->mNormalSpeed = 3.0f;
                p->current.angle.y = wallAngle;
                p->shape_angle.y = wallAngle;
            }
        }
    }
    *static_cast<int*>(result) = 1;
    return HOOK_SKIP_ORIGINAL;
}

}  // namespace

void initialize() {
    if (installed) return;
#if defined(TWILIGHT_BUNDLED_ANIMATION_BACKUP)
    const bool wallClipReady = loadClip(wallClip, "animations/wall_run.bck");
    const bool ledgeGrabClipReady = loadClip(ledgeGrabClip, "animations/ledge_grab.bck");
#else
    const bool wallClipReady = loadExternalClip(wallClip, "wall_run.bck");
    const bool ledgeGrabClipReady = loadExternalClip(ledgeGrabClip, "ledge_grab.bck");
#endif
    const bool clipsReady = wallClipReady && ledgeGrabClipReady;
    if (clipsReady) {
        const bool hooksOk =
            mods::hook::add_pre<Move>(movePre) == MOD_OK &&
            mods::hook::add_pre<Wait>(movePre) == MOD_OK &&
            mods::hook::add_pre<Jump>(jumpPre) == MOD_OK &&
            mods::hook::add_post<Execute>(executePost) == MOD_OK &&
            mods::hook::add_post<HangWallCatchInit>(hangWallCatchInitPost) == MOD_OK &&
            mods::hook::add_pre<HangWallCatch>(hangWallCatchPre) == MOD_OK;
        if (hooksOk) {
            installed = true;
            svc_log->info(mod_ctx, "SS wall running option initialized.");
            return;
        }
        mods::hook::uninstall<HangWallCatch>();
        mods::hook::uninstall<HangWallCatchInit>();
        mods::hook::uninstall<Execute>();
        mods::hook::uninstall<Jump>();
        mods::hook::uninstall<Wait>();
        mods::hook::uninstall<Move>();
    }
    freeClips();
    svc_log->warn(mod_ctx, clipsReady
        ? "SS wall running unavailable because one or more hooks could not be installed."
        : "SS wall running unavailable because one or more required animation files failed to load."
    );
}

void shutdown() {
    if (runner != nullptr) {
        runner->gravity = savedGravity;
        if (runner->mProcID == daAlink_c::PROC_AUTO_JUMP) runner->procFallInit(1, 3.0f);
        detachClip(runner);
    }
    remaining = 0;
    runner = nullptr;
    shortRun = false;
    ledgeGrabPending = false;
    shortWallRecovery = 0;
    requireRelease = false;
    diagnosticDelay = 0;
    if (installed) {
        mods::hook::uninstall<HangWallCatch>();
        mods::hook::uninstall<HangWallCatchInit>();
        mods::hook::uninstall<Execute>();
        mods::hook::uninstall<Jump>();
        mods::hook::uninstall<Wait>();
        mods::hook::uninstall<Move>();
        installed = false;
    }
    freeClips();
}

}  // namespace twilight_visuals::wall_run
