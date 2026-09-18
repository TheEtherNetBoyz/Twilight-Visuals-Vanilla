#pragma once

// Current Dusklight exposes typed helpers in <mods/svc/hook.hpp>; older SDKs
// expose the service wrappers in <mods/hook.hpp>.
#if __has_include("mods/svc/hook.hpp")
#include "mods/svc/hook.hpp"
#define TWILIGHT_CURRENT_HOOK_API 1
#else
#include "mods/hook.hpp"
#define TWILIGHT_CURRENT_HOOK_API 0
#endif

const HookService* twilight_hook_service();

#if !TWILIGHT_CURRENT_HOOK_API
namespace mods::hook {
template <class Entry>
ModResult add_pre(HookPreFn callback, const HookOptions* options = nullptr) {
    return mods::hook_add_pre<Entry>(twilight_hook_service(), callback, options);
}

template <class Entry>
ModResult add_post(HookPostFn callback, const HookOptions* options = nullptr) {
    return mods::hook_add_post<Entry>(twilight_hook_service(), callback, options);
}

template <class Entry>
void uninstall() {}
}  // namespace mods::hook
#endif
