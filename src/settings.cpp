#include "settings.hpp"
#include "native_face_tuner.hpp"

#include "mods/service.hpp"
#include "mods/svc/ui.h"
#include "d/actor/d_a_alink.h"

#include <algorithm>
#include <array>

IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

namespace twilight_visuals {
namespace {
Settings g_settings;
UiWindowHandle g_settingsWindow{};
UiWindowHandle g_faceWindow{};
UiMenuTabHandle g_quickMenuTab{};

constexpr std::array<const char*, 4> kStyles{
    "Normal Twilight", "Black and White", "Astral Plane", "The Dark Hour"};
constexpr std::array<const char*, 17> kSkyboxes{
    "Twilight Day", "Twilight Night", "Sunrise", "Sunset", "Overcast / Storm",
    "Faron Twilight", "Eldin Twilight", "Lanayru Twilight", "Palace of Twilight",
    "Sacred Grove", "Snowpeak", "Gerudo Desert", "Lake Hylia", "Fishing Hole",
    "Ordon", "Hyrule Field", "Castle Town"};
constexpr std::array<const char*, 9> kWeather{
    "Current", "Clear", "Rain", "Snow", "Lightning", "Wind Storm", "Snow Storm",
    "Heavy Fog", "Blood Rain"};
constexpr std::array<const char*, 4> kBloomModes{
    "Native Dusklight", "Off", "Classic (MFB)", "Dusklight"};
constexpr std::array<const char*, 4> kMenuScalingModes{
    "Native Dusklight", "GameCube (MFB)", "Wii (MFB)", "Dusklight (MFB)"};
constexpr std::array<const char*, 2> kLoadModes{"Normal", "Fast Loads (MFB)"};
constexpr std::array<const char*, 163> kFaceExpressions{
    "0 (Neutral)", "MABA01", "MABA02", "MABA03", "MABA01 L", "MABA01 R",
    "MABAGOMA", "DAM01", "FINISHA", "ARELORD", "ARELORDTAME", "PUSHW", "PULLW",
    "WAITST", "CUTST", "A WAITST", "WAITHDB", "WAITWATOWB", "CUTUNG", "CUTDL",
    "CUTDR", "SWIMINGA", "SWIMP", "SWIMDIVE", "GRABP", "GRABUP", "HEAVYTHROW",
    "GRABNG", "SWAITA", "PICKUP", "DOOROPA", "DOOROPB", "CUTHLA", "CUTHLB",
    "CUTHRA", "CUTHRB", "CUTHTB", "TURNBS", "ROLLFGOOD", "B A", "C A", "TURNBACK",
    "DAMFFUP", "DAMFBUP", "DAMFRLUP", "WAITATOS", "CUTA", "CUTL", "CUTR", "CUTF",
    "CUTEA", "CUTEB", "CUTEG", "CHANGEATOW", "CHANGEWTOA", "SWAITHA", "SWIATHB",
    "DASHWIND", "CUTTB", "CUTT", "CUTJST", "CUTJED", "BINDRINKST", "BINDRINK",
    "BINDRINKED", "BINBAD", "BINOP", "BINOUT", "BINFAIRY", "BINSWINGS", "BINSWINGU",
    "BINGET", "I BINGET", "K BINGET", "GRASSAST", "CATCHTAKA", "E A", "BOXOPSHORT",
    "BOXOPKICK", "BOXOP", "DIE", "DIEH", "SWIMDIEA", "SWIMDIEP", "ENTRANCE",
    "COWCATCHST", "COWTHROW L", "COWTHROW R", "DIEHUP", "CUTRE", "CUTU", "CUTUED",
    "CLIMBHANGMISS", "DAMFBW", "UNK 94", "UNK 95", "UNK 96", "UNK 97", "UNK 98",
    "UNK 99", "UNK 100", "UNK 101", "UNK 102", "UNK 103", "UNK 104", "UNK 105",
    "UNK 106", "UNK 107", "UNK 108", "SPILLH", "HANGH", "RODSWING", "RODSWINGL",
    "GETSWL", "TURNLS", "TURNRS", "KEYCATCHH", "DEMOTALKA", "DEMOTALKB", "DEMOTALKC",
    "CANORELEASE", "WAITINSECT", "I A", "J A", "K A", "ATDEFNG", "DEMOMHOP",
    "CUTEHST", "CUTEH", "CUTTJP", "CUTTJST", "CUTTJ", "CUTTJED", "UNK 133",
    "UNK 134", "UNK 135", "ODOROKU", "ASHIMOTO", "UNAZUKU", "WL MABA01",
    "WL MABA02", "WL SWAITA", "WL SWIMP", "WL SWAITB", "WL DAM", "WL B A",
    "WL DAMFFBUP", "WL DAMFLRUP", "WL WAITST", "WL LANDDAMA", "WL LANDDAMAST",
    "WL ATTACKUNG", "WL DASHWIND", "WL THROUGH", "WL ATTACKREST", "WL ATTACKREED",
    "WL DIE", "WL SWIMDIEA", "WL SWIMDIEP", "WL MDSHOCK", "WL ENTRANCE", "WL HOWLC",
    "WL C A"};
static_assert(kFaceExpressions.size() ==
              static_cast<size_t>(daAlink_c::FTANM_WL_C_A) + 1,
    "The facial-expression menu must contain every vanilla daAlink_FTANM entry.");

constexpr auto make_face_options() {
    std::array<const char*, kFaceExpressions.size() + 1> options{};
    options[0] = "Automatic (Game Controlled)";
    for (size_t i = 0; i < kFaceExpressions.size(); ++i) {
        options[i + 1] = kFaceExpressions[i];
    }
    return options;
}
constexpr auto kFaceOptions = make_face_options();

ModResult register_bool(const char* name, bool defaultValue, ConfigVarHandle& out) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = name;
    desc.type = CONFIG_VAR_BOOL;
    desc.default_bool = defaultValue;
    return svc_config->register_var(mod_ctx, &desc, &out);
}

ModResult register_int(
    const char* name, int64_t defaultValue, ConfigVarHandle& out) {
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = name;
    desc.type = CONFIG_VAR_INT;
    desc.default_int = defaultValue;
    return svc_config->register_var(mod_ctx, &desc, &out);
}

void add_control(UiElementHandle pane, UiControlDesc& control) {
    svc_ui->pane_add_control(mod_ctx, pane, &control, nullptr);
}

void open_face_tuner(ModContext*, void*);

void add_button(UiElementHandle pane, const char* label, const char* help,
    UiPressedFn onPressed) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_BUTTON;
    control.label = label;
    control.help_rml = help;
    control.on_pressed = onPressed;
    add_control(pane, control);
}

void add_toggle(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_TOGGLE;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    add_control(pane, control);
}

template <size_t N>
void add_select(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var,
    const std::array<const char*, N>& options) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_SELECT;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.options = options.data();
    control.option_count = options.size();
    add_control(pane, control);
}

void add_number(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var,
    int64_t min, int64_t max, int64_t step, const char* suffix) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = label;
    control.help_rml = help;
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = var;
    control.min = min;
    control.max = max;
    control.step = step;
    control.suffix = suffix;
    add_control(pane, control);
}

ModResult build_settings_tab(ModContext*, UiWindowHandle, UiElementHandle left,
    UiElementHandle right, void*, ModError*) {
    (void)right;
    svc_ui->pane_add_section(mod_ctx, left, "Twilight Visuals");
    add_toggle(left, "Enable Twilight Visuals",
        "Apply Twilight visuals, particles, enemy variants, and the selected style anywhere.",
        g_settings.enabled);
    add_select(left, "Visual Style & Music",
        "Select the complete environment style and its matching music. Normal Twilight and Black "
        "and White use Palace music; Astral Plane and The Dark Hour use their matching tracks.",
        g_settings.style,
        kStyles);
    add_number(left, "Brightness",
        "Adjust complete environment lighting and bloom intensity.", g_settings.brightness, 0,
        120, 5, "%");
    add_number(left, "Astral Chromatic Aberration",
        "Adjust Astral Plane red/blue edge separation.", g_settings.chromaticAberration, 0, 200,
        5, "%");
    add_select(left, "Skybox", "Choose the authored sky palette used by Twilight Visuals.",
        g_settings.skybox, kSkyboxes);
    add_toggle(left, "Exclude Palace of Twilight",
        "Keep the Palace of Twilight's native visuals and music instead of applying the selected "
        "Twilight Visuals preset.", g_settings.excludePalaceOfTwilight);

    svc_ui->pane_add_section(mod_ctx, left, "Music");
    add_number(left, "Custom Music Volume",
        "Adjust Astral Plane and Dark Hour replacement music volume.",
        g_settings.musicVolume, 0, 100, 5, "%");
    add_select(left, "Bloom Override Mode",
        "Choose the bloom implementation used by Twilight Visuals. Native Dusklight leaves "
        "Dusklight's own bloom setting in control.",
        g_settings.bloomMode, kBloomModes);
    add_number(left, "Bloom Brightness",
        "Adjust the MFB bloom brightness multiplier independently from scene brightness.",
        g_settings.bloomBrightness, 0, 100, 10, "%");
    add_toggle(left, "Override Temple Music",
        "Use the selected visual preset's music in temples and dungeons when custom music support "
        "is available. Boss and miniboss themes remain unchanged.",
        g_settings.overrideTempleMusic);

    svc_ui->pane_add_section(mod_ctx, left, "Weather");
    add_select(left, "Weather", "Override weather independently from the visual toggle.",
        g_settings.weather, kWeather);

    svc_ui->pane_add_section(mod_ctx, left, "Movement");
    add_toggle(left, "Skyward Sword Running",
        "Hold A while moving as human Link to run at 37 units. Includes the custom attack, roll, "
        "snow, and Magic Armor water-running behavior.",
        g_settings.skywardSwordRunning);
    add_toggle(left, "Put Sword Away When Sprinting",
        "Automatically sheath Link's sword once when Skyward Sword sprinting begins.",
        g_settings.sheathSwordWhileSprinting);
    add_toggle(left, "SS Wall Running",
        "Enable Skyward Sword-style wall running, short-wall step-up, and ledge-grab behavior "
        "while Skyward Sword Running is enabled.",
        g_settings.skywardSwordWallRunning);
    add_toggle(left, "Wolf Senses as Human",
        "Press D-pad Down as human Link to toggle Wolf Senses after the ability has been unlocked.",
        g_settings.humanWolfSenses);

    svc_ui->pane_add_section(mod_ctx, left, "Loading");
    add_select(left, "Load Mode",
        "Normal keeps vanilla transitions. Fast shortens area-transition waits and processes "
        "more loading work per frame. Reset, title, and protected story transitions stay "
        "vanilla. MFB Instant Loads are intentionally excluded because their full implementation "
        "requires engine-side actor and frame-pump changes that vanilla hooks cannot add safely.",
        g_settings.loadMode, kLoadModes);

    svc_ui->pane_add_section(mod_ctx, left, "Interface");
    add_toggle(left, "Hide Mouse Cursor During Gameplay",
        "Hide the mouse cursor while no Dusklight or in-game menu is visible. The cursor is "
        "restored whenever a menu opens.", g_settings.hideGameplayCursor);
    add_select(left, "Menu Scaling Override",
        "Use the native Dusklight menu scaling setting, or force the matching MFB GameCube, "
        "Wii, or Dusklight layout for the file select, collection, save, name, and TV-check "
        "menus. Native Dusklight leaves the host setting in control.",
        g_settings.menuScaling, kMenuScalingModes);
    add_button(left, "Facial Expression",
        "Open the bottom-screen facial-expression tuner. It includes Automatic plus every "
        "vanilla human and wolf daAlink_FTANM expression.",
        open_face_tuner);
    return MOD_OK;
}

void settings_window_closed(ModContext*, UiWindowHandle, void*) { g_settingsWindow = 0; }

void open_settings_window(ModContext*, void*) {
    if (g_settingsWindow != 0) return;
    UiTabDesc tabs[1] = {UI_TAB_DESC_INIT};
    tabs[0].title = "Twilight Visuals";
    tabs[0].build = build_settings_tab;
    UiWindowDesc window = UI_WINDOW_DESC_INIT;
    window.tabs = tabs;
    window.tab_count = 1;
    window.on_closed = settings_window_closed;
    svc_ui->window_push(mod_ctx, &window, &g_settingsWindow);
}

void get_face_option(ModContext*, void*, UiControlValue* out) {
    bool enabled = false;
    int64_t expression = 0;
    svc_config->get_bool(mod_ctx, g_settings.faceOverride, &enabled);
    svc_config->get_int(mod_ctx, g_settings.faceExpression, &expression);
    out->int_value = enabled ? std::clamp<int64_t>(expression, 0, 162) + 1 : 0;
}

void set_face_option(ModContext*, void*, const UiControlValue* value) {
    const int64_t selected = std::clamp<int64_t>(value->int_value, 0, 163);
    svc_config->set_bool(mod_ctx, g_settings.faceOverride, selected != 0);
    if (selected != 0) {
        svc_config->set_int(mod_ctx, g_settings.faceExpression, selected - 1);
    }
}

ModResult build_face_tuner(ModContext*, UiWindowHandle, UiElementHandle left,
    UiElementHandle, void*, ModError*) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_SELECT;
    control.label = "Facial Expression";
    control.help_rml =
        "Override Link's facial expression. Choose Automatic to return control to the game. "
        "Human and wolf expressions only apply to their matching form.";
    control.binding = UI_BINDING_CALLBACKS;
    control.get = get_face_option;
    control.set = set_face_option;
    control.options = kFaceOptions.data();
    control.option_count = kFaceOptions.size();
    add_control(left, control);
    return MOD_OK;
}

void face_window_closed(ModContext*, UiWindowHandle, void*) { g_faceWindow = 0; }

void open_face_tuner(ModContext*, void*) {
    native_face_tuner::open();
}

}  // namespace

Settings& settings() { return g_settings; }

bool get_bool(ConfigVarHandle handle, bool fallback) {
    bool value = fallback;
    return handle != 0 && svc_config->get_bool(mod_ctx, handle, &value) == MOD_OK ? value : fallback;
}

int64_t get_int(ConfigVarHandle handle, int64_t fallback) {
    int64_t value = fallback;
    return handle != 0 && svc_config->get_int(mod_ctx, handle, &value) == MOD_OK ? value : fallback;
}

ModResult register_settings(ModError*) {
    ModResult result = register_bool("twilight-visuals", false, g_settings.enabled);
    if (result != MOD_OK) return result;
    result = register_int("visual-style", 0, g_settings.style);
    if (result != MOD_OK) return result;
    result = register_int("brightness", 100, g_settings.brightness);
    if (result != MOD_OK) return result;
    result = register_int("chromatic-aberration", 80, g_settings.chromaticAberration);
    if (result != MOD_OK) return result;
    result = register_int("skybox", 0, g_settings.skybox);
    if (result != MOD_OK) return result;
    result = register_int("weather", 0, g_settings.weather);
    if (result != MOD_OK) return result;
    result = register_int("music-volume", 100, g_settings.musicVolume);
    if (result != MOD_OK) return result;
    result = register_int("bloom-mode", 0, g_settings.bloomMode);
    if (result != MOD_OK) return result;
    result = register_int("bloom-brightness", 100, g_settings.bloomBrightness);
    if (result != MOD_OK) return result;
    result = register_bool("legacy-bloom", false, g_settings.legacyBloom);
    if (result != MOD_OK) return result;
    result = register_bool("override-temple-music", false, g_settings.overrideTempleMusic);
    if (result != MOD_OK) return result;
    result = register_bool("skyward-sword-running", false, g_settings.skywardSwordRunning);
    if (result != MOD_OK) return result;
    result = register_bool("sheath-sword-while-sprinting", false,
                           g_settings.sheathSwordWhileSprinting);
    if (result != MOD_OK) return result;
    result = register_bool("skyward-sword-wall-running", false,
                           g_settings.skywardSwordWallRunning);
    if (result != MOD_OK) return result;
    result = register_bool("human-wolf-senses", false, g_settings.humanWolfSenses);
    if (result != MOD_OK) return result;
    result = register_bool("hide-gameplay-cursor", false, g_settings.hideGameplayCursor);
    if (result != MOD_OK) return result;
    result = register_int("menu-scaling", 0, g_settings.menuScaling);
    if (result != MOD_OK) return result;
    result = register_bool("face-override", false, g_settings.faceOverride);
    if (result != MOD_OK) return result;
    result = register_int("face-expression", 0, g_settings.faceExpression);
    if (result != MOD_OK) return result;
    result = register_int("load-mode", 0, g_settings.loadMode);
    if (result != MOD_OK) return result;
    return register_bool("exclude-palace-of-twilight", true,
                         g_settings.excludePalaceOfTwilight);
}

ModResult register_quick_menu_tab(ModError*) {
    UiMenuTabDesc tab = UI_MENU_TAB_DESC_INIT;
    tab.label = "Twilight Visuals";
    tab.on_selected = open_settings_window;
    return svc_ui->register_menu_tab(mod_ctx, &tab, &g_quickMenuTab);
}

void unregister_quick_menu_tab() {
    if (g_quickMenuTab != 0) {
        svc_ui->unregister_menu_tab(mod_ctx, g_quickMenuTab);
        g_quickMenuTab = 0;
    }
}

void close_settings_window() {
    if (g_faceWindow != 0) {
        svc_ui->window_close(mod_ctx, g_faceWindow);
        g_faceWindow = 0;
    }
    if (g_settingsWindow != 0) {
        svc_ui->window_close(mod_ctx, g_settingsWindow);
        g_settingsWindow = 0;
    }
}

}  // namespace twilight_visuals
