#include "facial.hpp"
#include "hook_api.hpp"
#include "runtime.hpp"

#include "d/actor/d_a_alink.h"
#include "mods/svc/hook.hpp"

#include <algorithm>

namespace twilight_visuals::facial {
namespace {
bool basicHookInstalled = false;
bool priorityHookInstalled = false;
bool playHookInstalled = false;
bool btpHookInstalled = false;
bool btkHookInstalled = false;
daAlink_c* lastPlayer = nullptr;
int lastExpression = -1;
bool lastOverride = false;

DEFINE_HOOK_SYMBOL("daAlink_c::setFaceBasicTexture",
    daAlink_c::daAlink_FTANM(daAlink_c*, daAlink_c::daAlink_FTANM), SetFaceBasicTexture);
DEFINE_HOOK_SYMBOL("daAlink_c::setFacePriTexture",
    void(daAlink_c*, daAlink_c::daAlink_FTANM), SetFacePriTexture);
DEFINE_HOOK_SYMBOL("daAlink_c::playFaceTextureAnime", void(daAlink_c*), PlayFaceTextureAnime);
DEFINE_HOOK_SYMBOL("daAlink_c::setFaceBtp", void(daAlink_c*, u16, BOOL, u16), SetFaceBtp);
DEFINE_HOOK_SYMBOL("daAlink_c::setFaceBtk", void(daAlink_c*, u16, BOOL, u16), SetFaceBtk);

void substitute_expression(void* args) {
    if (!runtime_settings().faceOverride) return;
    auto* player = mods::arg<daAlink_c*>(args, 0);
    if (player == nullptr) return;
    const int expression = std::clamp(runtime_settings().faceExpression, 0, 162);
    const bool expressionIsWolf = expression >= 138;
    if (expressionIsWolf != static_cast<bool>(player->checkWolf())) return;
    mods::arg_ref<daAlink_c::daAlink_FTANM>(args, 1) =
        static_cast<daAlink_c::daAlink_FTANM>(expression);
}

HookAction basic_pre(ModContext*, void* args, void*, void*) {
    substitute_expression(args);
    return HOOK_CONTINUE;
}

HookAction priority_pre(ModContext*, void* args, void*, void*) {
    substitute_expression(args);
    return HOOK_CONTINUE;
}

template <bool IsBtp>
HookAction face_resource_pre(ModContext*, void* args, void*, void*) {
    if (!runtime_settings().faceOverride) return HOOK_CONTINUE;
    auto* player = mods::arg<daAlink_c*>(args, 0);
    if (player == nullptr) return HOOK_CONTINUE;
    const int expression = std::clamp(runtime_settings().faceExpression, 0, 162);
    if ((expression >= 138) != static_cast<bool>(player->checkWolf())) return HOOK_CONTINUE;
    const auto* data = player->getFaceTexData(
        static_cast<daAlink_c::daAlink_FTANM>(expression));
    if (data != nullptr) {
        mods::arg_ref<u16>(args, 1) = IsBtp ? data->m_btpID : data->m_btkID;
    }
    return HOOK_CONTINUE;
}

void play_face_post(ModContext*, void* args, void*, void*) {
    auto* player = mods::arg<daAlink_c*>(args, 0);
    if (player == nullptr) return;

    const bool enabled = runtime_settings().faceOverride;
    const int expression = std::clamp(runtime_settings().faceExpression, 0, 162);
    const bool correctForm = (expression >= 138) == static_cast<bool>(player->checkWolf());
    if (!enabled || !correctForm) {
        lastPlayer = player;
        lastExpression = -1;
        lastOverride = enabled;
        return;
    }

    // Refresh only after Link's own face animation pass. Loading face resources from the
    // mod-loader tick races the animation heaps; doing a one-shot refresh here keeps all heap
    // mutation in the vanilla face-update phase and makes a newly selected expression visible.
    if (player != lastPlayer || expression != lastExpression || !lastOverride) {
        player->setFaceBasicTexture(static_cast<daAlink_c::daAlink_FTANM>(expression));
        lastPlayer = player;
        lastExpression = expression;
    }
    lastOverride = true;
}
}

ModResult initialize() {
    ModResult result = mods::hook::add_pre<SetFaceBasicTexture>(basic_pre);
    if (result != MOD_OK) return result;
    basicHookInstalled = true;
    result = mods::hook::add_pre<SetFacePriTexture>(priority_pre);
    if (result != MOD_OK) {
        mods::hook::uninstall<SetFaceBasicTexture>();
        basicHookInstalled = false;
        return result;
    }
    priorityHookInstalled = true;
    result = mods::hook::add_post<PlayFaceTextureAnime>(play_face_post);
    if (result != MOD_OK) {
        mods::hook::uninstall<SetFacePriTexture>();
        mods::hook::uninstall<SetFaceBasicTexture>();
        priorityHookInstalled = false;
        basicHookInstalled = false;
        return result;
    }
    playHookInstalled = true;
    result = mods::hook::add_pre<SetFaceBtp>(face_resource_pre<true>);
    if (result != MOD_OK) {
        mods::hook::uninstall<PlayFaceTextureAnime>();
        mods::hook::uninstall<SetFacePriTexture>();
        mods::hook::uninstall<SetFaceBasicTexture>();
        playHookInstalled = false;
        priorityHookInstalled = false;
        basicHookInstalled = false;
        return result;
    }
    btpHookInstalled = true;
    result = mods::hook::add_pre<SetFaceBtk>(face_resource_pre<false>);
    if (result != MOD_OK) {
        mods::hook::uninstall<SetFaceBtp>();
        mods::hook::uninstall<PlayFaceTextureAnime>();
        mods::hook::uninstall<SetFacePriTexture>();
        mods::hook::uninstall<SetFaceBasicTexture>();
        btpHookInstalled = false;
        playHookInstalled = false;
        priorityHookInstalled = false;
        basicHookInstalled = false;
        return result;
    }
    btkHookInstalled = true;
    return MOD_OK;
}

void update() {}

void shutdown() {
    if (btkHookInstalled) {
        mods::hook::uninstall<SetFaceBtk>();
        btkHookInstalled = false;
    }
    if (btpHookInstalled) {
        mods::hook::uninstall<SetFaceBtp>();
        btpHookInstalled = false;
    }
    if (playHookInstalled) {
        mods::hook::uninstall<PlayFaceTextureAnime>();
        playHookInstalled = false;
    }
    if (priorityHookInstalled) {
        mods::hook::uninstall<SetFacePriTexture>();
        priorityHookInstalled = false;
    }
    if (basicHookInstalled) {
        mods::hook::uninstall<SetFaceBasicTexture>();
        basicHookInstalled = false;
    }
    lastPlayer = nullptr;
    lastExpression = -1;
    lastOverride = false;
}
}
