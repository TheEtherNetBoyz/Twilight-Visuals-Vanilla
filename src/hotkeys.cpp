#include "hotkeys.hpp"

#include "hook_api.hpp"
#include "service_refs.hpp"
#include "settings.hpp"
#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "d/d_camera.h"
#include "dolphin/gx.h"
#include "aurora/texture.hpp"
#include <SDL3/SDL.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <array>
#include <memory>
#include <optional>
#include <vector>

struct GXTexObj_;

// Minimal ABI declarations for the replacement lookup return value. Keeping these
// local avoids depending on Aurora's private renderer/WebGPU headers.
namespace aurora::gfx {
struct TextureRef;
using TextureHandle = std::shared_ptr<TextureRef>;
namespace texture_replacement {
struct ReplacementResult {
    TextureHandle handle;
    uint64_t id = 0;
};
}  // namespace texture_replacement
}  // namespace aurora::gfx

extern const ConfigService* svc_config;
extern const UiService* svc_ui;

namespace twilight_visuals::hotkeys {
namespace {
using Replacement = aurora::gfx::texture_replacement::ReplacementResult;

DEFINE_HOOK_SYMBOL("dusk::gyro::queryGyroAimContext", bool(), GyroAimContext);
DEFINE_HOOK_SYMBOL("aurora::gfx::texture_replacement::find_pointer_replacement",
    std::optional<Replacement>(GXTexObj_&), FindPointerReplacement);
DEFINE_HOOK_SYMBOL("aurora::gfx::texture_replacement::find_source_replacement",
    std::optional<Replacement>(GXTexObj_&, aurora::texture::TextureSourceKey&),
    FindSourceReplacement);
DEFINE_HOOK_SYMBOL("aurora::gx::clear_static_texture_cache", void(),
    ClearStaticTextureCache);

std::array<bool, 3> s_keyboardWasDown{};
std::vector<SDL_JoystickID> s_gamepads;
uint32_t s_gamepadRefreshFrames = 0;
std::optional<Action> s_capturing;
UiElementHandle s_captureControl = 0;
bool s_waitingForRelease = false;
decltype(&SDL_GetKeyboardState) s_getKeyboardState = nullptr;
decltype(&SDL_GetScancodeName) s_getScancodeName = nullptr;
decltype(&SDL_GetGamepads) s_getGamepads = nullptr;
decltype(&SDL_GetGamepadFromID) s_getGamepadFromID = nullptr;
decltype(&SDL_GetGamepadButton) s_getGamepadButton = nullptr;
decltype(&SDL_GetGamepadAxis) s_getGamepadAxis = nullptr;
decltype(&SDL_free) s_sdlFree = nullptr;
bool s_gyroHook = false;
bool s_pointerHook = false;
bool s_sourceHook = false;
bool s_textureCacheHook = false;
UiStyleHandle s_hotkeyToastStyle = 0;

constexpr int64_t kControllerButtonBase = 1000;
constexpr int64_t kControllerAxisBase = 2000;
constexpr Sint16 kTriggerThreshold = 16000;

template <typename Function>
Function resolve_sdl_function(const char* name) {
#if defined(_WIN32)
    static HMODULE module = [] {
        HMODULE result = GetModuleHandleA("SDL3.dll");
        if (result == nullptr) result = GetModuleHandleA("SDL3-shared.dll");
        return result;
    }();
    return module == nullptr ? nullptr : reinterpret_cast<Function>(GetProcAddress(module, name));
#else
    return reinterpret_cast<Function>(dlsym(RTLD_DEFAULT, name));
#endif
}

ConfigVarHandle binding_var(Action action) {
    switch (action) {
    case Action::Gyro: return settings().hotkeyGyro;
    case Action::Bloom: return settings().hotkeyBloom;
    case Action::Textures: return settings().hotkeyTextures;
    }
    return 0;
}

const char* action_name(Action action) {
    switch (action) {
    case Action::Gyro: return "Gyro Aim";
    case Action::Bloom: return "Bloom Mode";
    case Action::Textures: return "Texture Replacements";
    }
    return "Hotkey";
}

void resolve_sdl() {
    s_getKeyboardState = resolve_sdl_function<decltype(s_getKeyboardState)>(
        "SDL_GetKeyboardState");
    s_getScancodeName = resolve_sdl_function<decltype(s_getScancodeName)>(
        "SDL_GetScancodeName");
    s_getGamepads = resolve_sdl_function<decltype(s_getGamepads)>("SDL_GetGamepads");
    s_getGamepadFromID = resolve_sdl_function<decltype(s_getGamepadFromID)>(
        "SDL_GetGamepadFromID");
    s_getGamepadButton = resolve_sdl_function<decltype(s_getGamepadButton)>(
        "SDL_GetGamepadButton");
    s_getGamepadAxis = resolve_sdl_function<decltype(s_getGamepadAxis)>(
        "SDL_GetGamepadAxis");
    s_sdlFree = resolve_sdl_function<decltype(s_sdlFree)>("SDL_free");
}

void refresh_gamepads() {
    if (s_getGamepads == nullptr || s_sdlFree == nullptr) return;
    int count = 0;
    SDL_JoystickID* ids = s_getGamepads(&count);
    s_gamepads.clear();
    if (ids != nullptr && count > 0) s_gamepads.assign(ids, ids + count);
    s_sdlFree(ids);
}

bool binding_down(int64_t value) {
    if (value <= 0) return false;
    if (value < kControllerButtonBase) {
        int count = 0;
        const bool* state = s_getKeyboardState != nullptr ? s_getKeyboardState(&count) : nullptr;
        const int code = static_cast<int>(value - 1);
        return state != nullptr && code > SDL_SCANCODE_UNKNOWN && code < count && state[code];
    }
    for (const SDL_JoystickID id : s_gamepads) {
        SDL_Gamepad* gamepad = s_getGamepadFromID != nullptr ? s_getGamepadFromID(id) : nullptr;
        if (gamepad == nullptr) continue;
        if (value >= kControllerAxisBase) {
            const auto axis = static_cast<SDL_GamepadAxis>(value - kControllerAxisBase);
            if (s_getGamepadAxis != nullptr && s_getGamepadAxis(gamepad, axis) > kTriggerThreshold)
                return true;
        } else {
            const auto button = static_cast<SDL_GamepadButton>(value - kControllerButtonBase);
            if (s_getGamepadButton != nullptr && s_getGamepadButton(gamepad, button)) return true;
        }
    }
    return false;
}

bool pressed(Action action, std::size_t index) {
    const bool down = binding_down(get_int(binding_var(action), 0));
    const bool edge = down && !s_keyboardWasDown[index];
    s_keyboardWasDown[index] = down;
    return edge;
}

const char* controller_button_name(SDL_GamepadButton button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "South / A";
    case SDL_GAMEPAD_BUTTON_EAST: return "East / B";
    case SDL_GAMEPAD_BUTTON_WEST: return "West / X";
    case SDL_GAMEPAD_BUTTON_NORTH: return "North / Y";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back / Select";
    case SDL_GAMEPAD_BUTTON_GUIDE: return "Guide / Home";
    case SDL_GAMEPAD_BUTTON_START: return "Start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "Left Stick Click";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "Right Stick Click";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "Left Shoulder";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "Right Shoulder";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
    case SDL_GAMEPAD_BUTTON_MISC1: return "Share / Capture";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "Right Paddle 1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "Left Paddle 1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "Right Paddle 2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "Left Paddle 2";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad Click";
    default: return "Controller Button";
    }
}

std::string value_name(int64_t value) {
    if (value <= 0) return "Unbound";
    if (value >= kControllerAxisBase)
        return value - kControllerAxisBase == SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                   ? "Left Trigger" : "Right Trigger";
    if (value >= kControllerButtonBase)
        return controller_button_name(
            static_cast<SDL_GamepadButton>(value - kControllerButtonBase));
    const auto code = static_cast<SDL_Scancode>(value - 1);
    const char* name = s_getScancodeName != nullptr ? s_getScancodeName(code) : nullptr;
    return name != nullptr && *name != '\0' ? name : "Keyboard Key";
}

int64_t first_pressed_input() {
    int count = 0;
    const bool* keys = s_getKeyboardState != nullptr ? s_getKeyboardState(&count) : nullptr;
    if (keys != nullptr) {
        for (int code = 1; code < count; ++code)
            if (keys[code]) return static_cast<int64_t>(code) + 1;
    }
    for (const SDL_JoystickID id : s_gamepads) {
        SDL_Gamepad* pad = s_getGamepadFromID != nullptr ? s_getGamepadFromID(id) : nullptr;
        if (pad == nullptr) continue;
        if (s_getGamepadButton != nullptr) {
            for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
                if (s_getGamepadButton(pad, static_cast<SDL_GamepadButton>(button)))
                    return kControllerButtonBase + button;
        }
        if (s_getGamepadAxis != nullptr) {
            if (s_getGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerThreshold)
                return kControllerAxisBase + SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
            if (s_getGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerThreshold)
                return kControllerAxisBase + SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        }
    }
    return 0;
}

void update_capture() {
    const int64_t input = first_pressed_input();
    if (s_waitingForRelease) {
        if (input == 0) s_waitingForRelease = false;
        return;
    }
    if (input == 0 || !s_capturing.has_value()) return;
    const Action action = *s_capturing;

    if (input == static_cast<int64_t>(SDL_SCANCODE_ESCAPE) + 1) {
        svc_config->set_int(mod_ctx, binding_var(action), 0);
        const std::string label = std::string(action_name(action)) + ": Unbound";
        if (s_captureControl != 0)
            svc_ui->control_set_label(mod_ctx, s_captureControl, label.c_str());
        UiToastDesc toast = UI_TOAST_DESC_INIT;
        const std::string body = std::string(action_name(action)) + " hotkey unbound.";
        toast.title_rml = "Hotkey cleared";
        toast.body_rml = body.c_str();
        toast.duration_ms = 2500;
        svc_ui->push_toast(mod_ctx, &toast);
        s_capturing.reset();
        s_captureControl = 0;
        s_keyboardWasDown = {};
        return;
    }

    svc_config->set_int(mod_ctx, binding_var(action), input);
    const std::string name = value_name(input);
    const std::string label = std::string(action_name(action)) + ": " + name;
    if (s_captureControl != 0) svc_ui->control_set_label(mod_ctx, s_captureControl, label.c_str());
    UiToastDesc toast = UI_TOAST_DESC_INIT;
    const std::string body = std::string(action_name(action)) + " bound to " + name + ".";
    toast.title_rml = "Hotkey saved";
    toast.body_rml = body.c_str();
    toast.duration_ms = 2500;
    svc_ui->push_toast(mod_ctx, &toast);
    s_capturing.reset();
    s_captureControl = 0;
    s_keyboardWasDown = {};
}

void show_activation(Action action, const std::string& state) {
    if (svc_ui == nullptr || !SERVICE_HAS(svc_ui, UiService, push_toast)) return;

    UiToastDesc toast = UI_TOAST_DESC_INIT;
    const std::string title = action_name(action);
    toast.type = "twilight-hotkey";
    toast.title_rml = title.c_str();
    toast.body_rml = state.c_str();
    toast.duration_ms = 1800;
    svc_ui->push_toast(mod_ctx, &toast);
}

const char* bloom_mode_name(int64_t mode) {
    switch (mode) {
    case 0: return "Native Dusklight";
    case 1: return "Off";
    case 2: return "Classic (MFB)";
    case 3: return "Dusklight";
    default: return "Unknown";
    }
}

void invalidate_texture_caches() {
    // GXInvalidateTexAll is intentionally a no-op in Aurora. The existing Aurora
    // cache-clear entry point is the mod-safe way to force the next draw to run
    // through the replacement lookup again.
    GXInvalidateTexAll();
    if (s_textureCacheHook && ClearStaticTextureCache::g_orig != nullptr)
        ClearStaticTextureCache::g_orig();
}

HookAction gyro_pre(ModContext*, void*, void* retval, void*) {
    if (retval == nullptr) return HOOK_CONTINUE;
    *static_cast<bool*>(retval) = get_bool(settings().hotkeyGyroEnabled, true) &&
                                  dCamera_c::isAimActive();
    return HOOK_SKIP_ORIGINAL;
}

HookAction texture_cache_clear_pre(ModContext*, void*, void*, void*) {
    return HOOK_CONTINUE;
}

HookAction filter_replacement_pre(ModContext*, void*, void* retval, void*) {
    if (get_bool(settings().hotkeyTexturesEnabled, true)) return HOOK_CONTINUE;
    if (retval != nullptr) static_cast<std::optional<Replacement>*>(retval)->reset();
    return HOOK_SKIP_ORIGINAL;
}

HookAction filter_pointer_pre(ModContext* ctx, void* args, void* retval, void* userData) {
    return filter_replacement_pre(ctx, args, retval, userData);
}

HookAction filter_source_pre(ModContext* ctx, void* args, void* retval, void* userData) {
    return filter_replacement_pre(ctx, args, retval, userData);
}
}  // namespace

ModResult initialize() {
    resolve_sdl();
    refresh_gamepads();
    if (s_getKeyboardState == nullptr)
        svc_log->warn(mod_ctx, "Keyboard hotkeys unavailable: SDL input API was not found.");
    if (s_getGamepads == nullptr || s_getGamepadFromID == nullptr ||
        s_getGamepadButton == nullptr || s_getGamepadAxis == nullptr)
        svc_log->warn(mod_ctx, "Controller hotkeys unavailable: SDL gamepad API was not found.");
    s_gyroHook = mods::hook::add_pre<GyroAimContext>(gyro_pre) == MOD_OK;
    s_pointerHook = mods::hook::add_pre<FindPointerReplacement>(filter_pointer_pre) == MOD_OK;
    s_sourceHook = mods::hook::add_pre<FindSourceReplacement>(filter_source_pre) == MOD_OK;
    s_textureCacheHook = mods::hook::add_pre<ClearStaticTextureCache>(texture_cache_clear_pre) == MOD_OK;
    if (SERVICE_HAS(svc_ui, UiService, register_styles_file)) {
        const ModResult styleResult = svc_ui->register_styles_file(
            mod_ctx, UI_SCOPE_OVERLAY, "hotkey_toast.rcss", &s_hotkeyToastStyle);
        if (styleResult != MOD_OK)
            svc_log->warn(mod_ctx, "Hotkey popup style could not be registered.");
    }
    if (!s_gyroHook) svc_log->warn(mod_ctx, "Gyro hotkey hook unavailable on this Dusklight build.");
    if (!s_pointerHook || !s_sourceHook)
        svc_log->warn(mod_ctx, "Texture replacement hotkey hook unavailable on this Dusklight build.");
    if (s_pointerHook || s_sourceHook)
        svc_log->info(mod_ctx, "Texture replacement hotkey compatibility hook active.");
    if (!s_textureCacheHook)
        svc_log->warn(mod_ctx, "Texture cache clear hook unavailable; texture toggles may need a room reload.");
    return MOD_OK;
}

void begin_binding(Action action, UiElementHandle control) {
    s_capturing = action;
    s_captureControl = control;
    s_waitingForRelease = true;
    if (control != 0)
        svc_ui->control_set_label(mod_ctx, control,
            "Release, then press a key/button (Esc clears)...");
}

std::string binding_label(Action action) {
    return std::string(action_name(action)) + ": " +
           value_name(get_int(binding_var(action), 0));
}

void update() {
    if (++s_gamepadRefreshFrames >= 120) {
        refresh_gamepads();
        s_gamepadRefreshFrames = 0;
    }
    if (s_capturing.has_value()) {
        update_capture();
        return;
    }
    if (pressed(Action::Gyro, 0) && s_gyroHook) {
        const bool next = !get_bool(settings().hotkeyGyroEnabled, true);
        svc_config->set_bool(mod_ctx, settings().hotkeyGyroEnabled, next);
        const std::string state = next ? "On" : "Off";
        show_activation(Action::Gyro, state);
        svc_log->info(mod_ctx, next ? "Gyro Aim hotkey: On" : "Gyro Aim hotkey: Off");
    }
    if (pressed(Action::Bloom, 1)) {
        const int64_t next = (get_int(settings().bloomMode, 0) + 1) % 4;
        svc_config->set_int(mod_ctx, settings().bloomMode, next);
        show_activation(Action::Bloom, bloom_mode_name(next));
        svc_log->info(mod_ctx, "Bloom mode cycled by hotkey.");
    }
    if (pressed(Action::Textures, 2)) {
        const bool next = !get_bool(settings().hotkeyTexturesEnabled, true);
        svc_config->set_bool(mod_ctx, settings().hotkeyTexturesEnabled, next);
        invalidate_texture_caches();
        show_activation(Action::Textures, next ? "On" : "Off");
        svc_log->info(mod_ctx,
            next ? "Texture replacements hotkey: On" : "Texture replacements hotkey: Off");
    }
}

void shutdown() {
    if (s_hotkeyToastStyle != 0 && SERVICE_HAS(svc_ui, UiService, unregister_styles))
        svc_ui->unregister_styles(mod_ctx, s_hotkeyToastStyle);
    s_hotkeyToastStyle = 0;
    if (s_sourceHook) mods::hook::uninstall<FindSourceReplacement>();
    if (s_pointerHook) mods::hook::uninstall<FindPointerReplacement>();
    if (s_textureCacheHook) mods::hook::uninstall<ClearStaticTextureCache>();
    if (s_gyroHook) mods::hook::uninstall<GyroAimContext>();
    s_sourceHook = s_pointerHook = s_gyroHook = s_textureCacheHook = false;
    s_keyboardWasDown = {};
    s_gamepads.clear();
    s_capturing.reset();
    s_captureControl = 0;
    s_waitingForRelease = false;
    s_gamepadRefreshFrames = 0;
    s_getKeyboardState = nullptr;
    s_getScancodeName = nullptr;
    s_getGamepads = nullptr;
    s_getGamepadFromID = nullptr;
    s_getGamepadButton = nullptr;
    s_getGamepadAxis = nullptr;
    s_sdlFree = nullptr;
}

}  // namespace twilight_visuals::hotkeys
