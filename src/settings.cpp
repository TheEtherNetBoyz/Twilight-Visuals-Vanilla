#include "settings.hpp"
#include "native_face_tuner.hpp"
#include "hotkeys.hpp"
#include "service_refs.hpp"

#include "mods/service.hpp"
#include "mods/svc/ui.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

namespace twilight_visuals {
namespace {
Settings g_settings;
UiWindowHandle g_settingsWindow{};
UiWindowHandle g_faceWindow{};
UiWindowHandle g_betterBloomWindow{};
UiMenuTabHandle g_quickMenuTab{};
UiElementHandle g_styleGroupControl{};
UiElementHandle g_chromaticAberrationControl{};
ConfigSubscriptionHandle g_areaBrightnessSubscription{};
std::string g_areaEditorKey;
std::map<std::string, int64_t> g_areaBrightness;

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
constexpr std::array<const char*, 6> kBloomModes{
    "Off (No Override)", "No Bloom", "Nvidia Shield", "Dusklight", "Classic (MFB)",
    "Better Bloom Settings"};
constexpr std::array<const char*, 3> kBloomBlendModes{
    "Area Controlled", "Additive", "Screen"};
constexpr std::array<const char*, 2> kBloomGainSources{"Vanilla", "Fixed"};
constexpr std::array<const char*, 2> kBloomTintSources{"Vanilla", "Neutral"};
constexpr std::array<const char*, 2> kBloomActivations{"Vanilla", "Everywhere"};
constexpr std::array<const char*, 3> kBloomThresholdSources{"Vanilla", "Custom", "Normalized"};
constexpr std::array<const char*, 3> kBloomResolutions{"Full", "Half", "Quarter"};
constexpr std::array<const char*, 7> kBloomPresets{
    "Custom", "Faithful", "Faithful (Cold)", "Modern", "Modern (Cold)", "Hybrid", "Haze"};
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
void open_better_bloom_settings(ModContext*, void*);
void get_dark_hour_fog_value(ModContext*, void*, UiControlValue* outValue);
void adjust_dark_hour_fog_value(ModContext*, void*, const UiControlValue* value);
ModResult build_visual_style_group(
    ModContext*, UiElementHandle, void*, ModError*);
ModResult build_brightness_group(
    ModContext*, UiElementHandle, void*, ModError*);
ModResult build_skybox_weather_group(
    ModContext*, UiElementHandle, void*, ModError*);
ModResult build_better_bloom_group(
    ModContext*, UiElementHandle, void*, ModError*);

struct StyleButtonContext {
    int64_t value;
};

std::array<StyleButtonContext, kStyles.size()> g_styleButtons{{{0}, {1}, {2}, {3}}};
std::string g_styleGroupLabel;

void refresh_style_group_label() {
    const int64_t style = std::clamp<int64_t>(get_int(g_settings.style, 0), 0,
                                               kStyles.size() - 1);
    g_styleGroupLabel = std::string("Visual Style & Music • ") + kStyles[style];
    if (g_styleGroupControl != 0 &&
        SERVICE_HAS(svc_ui, UiService, control_set_label)) {
        svc_ui->control_set_label(mod_ctx, g_styleGroupControl, g_styleGroupLabel.c_str());
    }
}

bool style_button_selected(ModContext*, void* userData) {
    const auto* context = static_cast<const StyleButtonContext*>(userData);
    return context != nullptr && get_int(g_settings.style, 0) == context->value;
}

void select_visual_style(ModContext*, void* userData) {
    const auto* context = static_cast<const StyleButtonContext*>(userData);
    if (context == nullptr) return;
    svc_config->set_int(mod_ctx, g_settings.style, context->value);
    refresh_style_group_label();
    if (g_chromaticAberrationControl != 0 &&
        SERVICE_HAS(svc_ui, UiService, elem_set_visible)) {
        svc_ui->elem_set_visible(mod_ctx, g_chromaticAberrationControl,
                                 context->value == 2);
    }
}

void get_bloom_mode(ModContext*, void*, UiControlValue* outValue) {
    if (outValue != nullptr) outValue->int_value = get_int(g_settings.bloomMode, 0);
}

void set_bloom_mode(ModContext*, void*, const UiControlValue* value) {
    if (value == nullptr) return;
    const int64_t mode = std::clamp<int64_t>(value->int_value, 0, 5);
    svc_config->set_int(mod_ctx, g_settings.bloomMode, mode);
    if (mode == 5) open_better_bloom_settings(mod_ctx, nullptr);
}

void add_button(UiElementHandle pane, const char* label, const char* help,
    UiPressedFn onPressed) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_BUTTON;
    control.label = label;
    control.help_rml = help;
    control.on_pressed = onPressed;
    add_control(pane, control);
}

struct HotkeyButtonContext {
    hotkeys::Action action;
    UiElementHandle control{};
    std::string label;
};

std::array<HotkeyButtonContext, 3> g_hotkeyButtons{{
    {hotkeys::Action::Gyro}, {hotkeys::Action::Bloom}, {hotkeys::Action::Textures}}};

void begin_hotkey_binding(ModContext*, void* userData) {
    auto* context = static_cast<HotkeyButtonContext*>(userData);
    if (context != nullptr) hotkeys::begin_binding(context->action, context->control);
}

void add_hotkey_button(UiElementHandle pane, HotkeyButtonContext& context, const char* help) {
    context.label = hotkeys::binding_label(context.action);
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_BUTTON;
    control.label = context.label.c_str();
    control.help_rml = help;
    control.on_pressed = begin_hotkey_binding;
    control.user_data = &context;
    svc_ui->pane_add_control(mod_ctx, pane, &control, &context.control);
}

ModResult build_hotkeys_group(ModContext*, UiElementHandle pane, void*, ModError*) {
    const char* help =
        "Select, release the activating input, then press any keyboard key or controller button. "
        "Press Escape to clear the binding.";
    add_hotkey_button(pane, g_hotkeyButtons[0], help);
    add_hotkey_button(pane, g_hotkeyButtons[1], help);
    add_hotkey_button(pane, g_hotkeyButtons[2], help);
    return MOD_OK;
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

template <size_t N>
void add_dropdown(UiElementHandle pane, const char* label, const char* help, ConfigVarHandle var,
    const std::array<const char*, N>& options) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_DROPDOWN;
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

void add_dark_hour_fog_tuner(UiElementHandle pane) {
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = "Foreground Visibility";
    control.help_rml =
        "Controls how far foreground terrain remains visible in every visual style. "
        "0% preserves the original fog. Adjust this setting to open the full-screen "
        "graphics tuner.";
    control.binding = UI_BINDING_CALLBACKS;
    control.get = get_dark_hour_fog_value;
    control.set = adjust_dark_hour_fog_value;
    control.min = 0;
    control.max = 400;
    control.step = 5;
    control.suffix = "%";
    add_control(pane, control);
}

std::string current_area_brightness_key() {
    const char* stage = dComIfGp_getStartStageName();
    if (stage == nullptr || *stage == '\0') return {};
    int room = dComIfGp_roomControl_getStayNo();
    if (room < 0) room = dComIfGp_getStartStageRoomNo();
    if (room < 0) return {};
    return std::string(stage) + ":" + std::to_string(room);
}

std::filesystem::path area_brightness_path() {
    const char* data_directory = nullptr;
    if (svc_host == nullptr || svc_host->data_dir == nullptr ||
        svc_host->data_dir(mod_ctx, &data_directory) != MOD_OK || data_directory == nullptr)
        return {};
    return std::filesystem::path(data_directory) / "twilight_visuals_area_brightness.cfg";
}

void load_area_brightness() {
    g_areaBrightness.clear();
    const auto path = area_brightness_path();
    if (path.empty()) return;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = line.substr(0, equals);
        const std::string text = line.substr(equals + 1);
        char* end = nullptr;
        const long long value = std::strtoll(text.c_str(), &end, 10);
        if (end != text.c_str() && *end == '\0')
            g_areaBrightness[key] = std::clamp<int64_t>(value, 25, 150);
    }
}

void save_area_brightness() {
    const auto path = area_brightness_path();
    if (path.empty()) return;
    std::ofstream file(path, std::ios::trunc);
    if (!file) return;
    file << "# Twilight Visuals per-area brightness (stage:room=percent)\n";
    for (const auto& [key, value] : g_areaBrightness) file << key << '=' << value << '\n';
}

void area_brightness_changed(ModContext*, ConfigVarHandle,
                             const ConfigVarValue* value, const ConfigVarValue*, void*) {
    if (value == nullptr || value->type != CONFIG_VAR_INT || g_areaEditorKey.empty()) return;
    g_areaBrightness[g_areaEditorKey] = std::clamp<int64_t>(value->int_value, 25, 150);
    save_area_brightness();
}

void prepare_current_area_brightness_editor() {
    g_areaEditorKey = current_area_brightness_key();
    const auto it = g_areaBrightness.find(g_areaEditorKey);
    const int64_t value = it == g_areaBrightness.end() ? 100 : it->second;
    svc_config->set_int(mod_ctx, g_settings.currentAreaBrightness, value);
}

void commit_current_area_brightness_editor() {
    if (g_areaEditorKey.empty()) return;
    const int64_t value = std::clamp<int64_t>(
        get_int(g_settings.currentAreaBrightness, 100), 25, 150);
    g_areaBrightness[g_areaEditorKey] = value;
    save_area_brightness();
}

void sync_current_area_brightness_editor() {
    if (g_settingsWindow == 0) return;
    const std::string key = current_area_brightness_key();
    if (key == g_areaEditorKey) return;

    // Preserve edits for the room being left, then make the live menu control
    // represent the newly loaded stage/room instead of retaining stale text.
    commit_current_area_brightness_editor();
    g_areaEditorKey = key;
    const auto it = g_areaBrightness.find(key);
    const int64_t value = it == g_areaBrightness.end() ? 100 : it->second;
    svc_config->set_int(mod_ctx, g_settings.currentAreaBrightness, value);
}

ModResult build_settings_tab(ModContext*, UiWindowHandle, UiElementHandle left,
    UiElementHandle right, void*, ModError*) {
    svc_ui->pane_add_section(mod_ctx, left, "Twilight Visuals");
    add_toggle(left, "Enable Mod",
        "Apply Twilight visuals, particles, enemy variants, and the selected style anywhere.",
        g_settings.enabled);
    add_toggle(left, "Enable Visual Style & Music",
        "Enable the selected visual style's lighting, tint, bloom, sky, particles, visual post-processing, "
        "and custom music. The master mod toggle and non-visual features remain independent.",
        g_settings.visualEffects);
    refresh_style_group_label();
    UiGroupDesc visualStyle = UI_GROUP_DESC_INIT;
    visualStyle.label = g_styleGroupLabel.c_str();
    visualStyle.build = build_visual_style_group;
    svc_ui->pane_add_group(mod_ctx, left, right, &visualStyle, &g_styleGroupControl);
    add_toggle(left, "Twilight Camera Light",
        "Use Twilight's camera-following spotlight. Disable this to test without the moving "
        "bright and dark bands while retaining the sky, weather, particles, bloom, and static lighting.",
        g_settings.twilightCameraLight);
    UiGroupDesc brightness = UI_GROUP_DESC_INIT;
    brightness.label = "Brightness Settings";
    brightness.build = build_brightness_group;
    svc_ui->pane_add_group(mod_ctx, left, right, &brightness, nullptr);
    UiGroupDesc skyboxWeather = UI_GROUP_DESC_INIT;
    skyboxWeather.label = "Skybox & Weather";
    skyboxWeather.build = build_skybox_weather_group;
    svc_ui->pane_add_group(mod_ctx, left, right, &skyboxWeather, nullptr);
    add_toggle(left, "Exclude Palace of Twilight",
        "Keep the Palace of Twilight's native visuals and music instead of applying the selected "
        "Twilight Visuals preset.", g_settings.excludePalaceOfTwilight);

    svc_ui->pane_add_section(mod_ctx, left, "Music");
    add_number(left, "Custom Music Volume",
        "Adjust Astral Plane and Dark Hour replacement music volume.",
        g_settings.musicVolume, 0, 100, 5, "%");
    add_toggle(left, "Override Temple Music",
        "Use the selected visual preset's music in temples and dungeons when custom music support "
        "is available. Boss and miniboss themes remain unchanged.",
        g_settings.overrideTempleMusic);

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

    svc_ui->pane_add_section(mod_ctx, left, "MFB Settings");
    UiControlDesc bloomMode = UI_CONTROL_DESC_INIT;
    bloomMode.kind = UI_CONTROL_SELECT;
    bloomMode.label = "Bloom Override Mode";
    bloomMode.help_rml =
        "Choose the bloom implementation used by Twilight Visuals. Native Dusklight leaves "
        "Dusklight's own bloom setting in control. Classic (MFB) renders Classic bloom at "
        "high resolution. Selecting Better Bloom Settings opens its dedicated controls.";
    bloomMode.binding = UI_BINDING_CALLBACKS;
    bloomMode.get = get_bloom_mode;
    bloomMode.set = set_bloom_mode;
    bloomMode.options = kBloomModes.data();
    bloomMode.option_count = kBloomModes.size();
    add_control(left, bloomMode);
    add_number(left, "Bloom Brightness",
        "Adjust the MFB bloom brightness multiplier independently from scene brightness.",
        g_settings.bloomBrightness, 0, 100, 10, "%");
    add_dark_hour_fog_tuner(left);
    add_select(left, "Load Mode",
        "Normal keeps vanilla transitions. Fast shortens area-transition waits and processes "
        "more loading work per frame. Reset, title, and protected story transitions stay "
        "vanilla. MFB Instant Loads are intentionally excluded because their full implementation "
        "requires engine-side actor and frame-pump changes that vanilla hooks cannot add safely.",
        g_settings.loadMode, kLoadModes);

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
    UiGroupDesc hotkeys = UI_GROUP_DESC_INIT;
    hotkeys.label = "Hotkeys";
    hotkeys.build = build_hotkeys_group;
    svc_ui->pane_add_group(mod_ctx, left, right, &hotkeys, nullptr);
    return MOD_OK;
}

ModResult build_visual_style_group(
    ModContext*, UiElementHandle pane, void*, ModError*) {
    g_chromaticAberrationControl = 0;
    for (size_t i = 0; i < kStyles.size(); ++i) {
        UiControlDesc style = UI_CONTROL_DESC_INIT;
        style.kind = UI_CONTROL_BUTTON;
        style.label = kStyles[i];
        style.help_rml =
            "Select the complete environment style and its matching music.";
        style.on_pressed = select_visual_style;
        style.is_selected = style_button_selected;
        style.user_data = &g_styleButtons[i];
        add_control(pane, style);
    }

    svc_ui->pane_add_text(mod_ctx, pane,
        "Normal Twilight and Black and White use Palace music; Astral Plane and The Dark Hour "
        "use their matching tracks.", nullptr);

    UiControlDesc aberration = UI_CONTROL_DESC_INIT;
    aberration.kind = UI_CONTROL_NUMBER;
    aberration.label = "Astral Chromatic Aberration";
    aberration.help_rml = "Adjust Astral Plane red/blue edge separation.";
    aberration.binding = UI_BINDING_CONFIG_VAR;
    aberration.config_var = g_settings.chromaticAberration;
    aberration.min = 0;
    aberration.max = 200;
    aberration.step = 5;
    aberration.suffix = "%";
    const ModResult result = svc_ui->pane_add_control(
        mod_ctx, pane, &aberration, &g_chromaticAberrationControl);
    if (result != MOD_OK) return result;
    if (SERVICE_HAS(svc_ui, UiService, elem_set_visible)) {
        svc_ui->elem_set_visible(mod_ctx, g_chromaticAberrationControl,
                                 get_int(g_settings.style, 0) == 2);
    }
    return MOD_OK;
}

ModResult build_brightness_group(
    ModContext*, UiElementHandle pane, void*, ModError*) {
    add_number(pane, "Brightness",
        "Adjust complete environment lighting and bloom intensity.", g_settings.brightness, 0,
        120, 5, "%");
    add_toggle(pane, "Per-Area Brightness",
        "Apply the separately saved brightness percentage for the loaded area.",
        g_settings.perAreaBrightness);
    add_number(pane, "Current Area Brightness",
        "Adjust the loaded stage and room. The displayed value follows room changes and is "
        "saved in the separate per-area brightness file.",
        g_settings.currentAreaBrightness, 25, 150, 5, "%");
    return MOD_OK;
}

ModResult build_skybox_weather_group(
    ModContext*, UiElementHandle pane, void*, ModError*) {
    add_dropdown(pane, "Skybox",
        "Choose the authored sky palette used by every Twilight Visuals style.",
        g_settings.skybox, kSkyboxes);
    add_dropdown(pane, "Weather",
        "Override weather independently from the visual toggle.",
        g_settings.weather, kWeather);
    return MOD_OK;
}

void set_bloom_profile(int64_t intensity, int64_t size, int64_t diffusion,
    int64_t saturation, int64_t response, int64_t darkening) {
    svc_config->set_int(mod_ctx, g_settings.betterBloomIntensity, intensity);
    svc_config->set_int(mod_ctx, g_settings.bloomSize, size);
    svc_config->set_int(mod_ctx, g_settings.bloomDiffusion, diffusion);
    svc_config->set_int(mod_ctx, g_settings.bloomSaturation, saturation);
    svc_config->set_int(mod_ctx, g_settings.bloomResponseGain, response);
    svc_config->set_int(mod_ctx, g_settings.bloomSceneDarkening, darkening);
}

void bloom_preset_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
    const ConfigVarValue*, void*) {
    if (value == nullptr || value->type != CONFIG_VAR_INT) return;
    if (value->int_value < 1 || value->int_value > 6) return;

    // Begin from Better Bloom 2.9.1's option defaults. Presets intentionally leave
    // activation, threshold overrides, blend mode and resolution untouched.
    set_bloom_profile(100, 100, 0, 100, 100, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 50);
    svc_config->set_int(mod_ctx, g_settings.bloomCoreWidth, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomCoreContribution, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomInnerWidth, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomInnerContribution, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomOuterWidth, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomOuterContribution, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomHaloWidth, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomHaloContribution, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomHighlightRolloff, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomHighlightLimit, 0);
    svc_config->set_int(mod_ctx, g_settings.bloomHighlightDesaturation, 0);
    svc_config->set_int(mod_ctx, g_settings.bloomTintSource, 0);
    svc_config->set_int(mod_ctx, g_settings.bloomTintRed, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomTintGreen, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomTintBlue, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomColourLayerStrength, 100);
    svc_config->set_bool(mod_ctx, g_settings.bloomPulseEnabled, true);
    switch (value->int_value) {
    case 1: // Faithful
    case 2: // Faithful (Cold)
        set_bloom_profile(600, 100, 73, 100, 24, 100);
        svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomCoreWidth, 116);
        svc_config->set_int(mod_ctx, g_settings.bloomInnerWidth, 451);
        svc_config->set_int(mod_ctx, g_settings.bloomInnerContribution, 17);
        svc_config->set_int(mod_ctx, g_settings.bloomOuterContribution, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomHaloContribution, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightRolloff, 176);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightLimit, 95);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightDesaturation, 100);
        if (value->int_value == 2) {
            svc_config->set_int(mod_ctx, g_settings.bloomTintSource, 1);
            svc_config->set_int(mod_ctx, g_settings.bloomTintRed, 52);
            svc_config->set_int(mod_ctx, g_settings.bloomTintGreen, 47);
            svc_config->set_int(mod_ctx, g_settings.bloomTintBlue, 44);
        }
        break;
    case 3: // Modern
        set_bloom_profile(129, 50, 29, 100, 40, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 47);
        svc_config->set_int(mod_ctx, g_settings.bloomColourLayerStrength, 0);
        break;
    case 4: // Modern (Cold)
        set_bloom_profile(100, 50, 29, 100, 50, 100);
        svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 50);
        svc_config->set_int(mod_ctx, g_settings.bloomColourLayerStrength, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomTintSource, 1);
        svc_config->set_int(mod_ctx, g_settings.bloomTintRed, 100);
        svc_config->set_int(mod_ctx, g_settings.bloomTintGreen, 96);
        svc_config->set_int(mod_ctx, g_settings.bloomTintBlue, 91);
        svc_config->set_bool(mod_ctx, g_settings.bloomPulseEnabled, false);
        break;
    case 5: // Hybrid
        set_bloom_profile(680, 100, 73, 100, 24, 100);
        svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomCoreWidth, 116);
        svc_config->set_int(mod_ctx, g_settings.bloomInnerWidth, 451);
        svc_config->set_int(mod_ctx, g_settings.bloomInnerContribution, 17);
        svc_config->set_int(mod_ctx, g_settings.bloomOuterWidth, 200);
        svc_config->set_int(mod_ctx, g_settings.bloomOuterContribution, 20);
        svc_config->set_int(mod_ctx, g_settings.bloomHaloContribution, 8);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightRolloff, 176);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightLimit, 95);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightDesaturation, 100);
        break;
    case 6: // Haze
        set_bloom_profile(135, 100, 45, 85, 40, 0);
        svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 70);
        svc_config->set_int(mod_ctx, g_settings.bloomCoreContribution, 15);
        svc_config->set_int(mod_ctx, g_settings.bloomInnerWidth, 120);
        svc_config->set_int(mod_ctx, g_settings.bloomInnerContribution, 40);
        svc_config->set_int(mod_ctx, g_settings.bloomOuterWidth, 150);
        svc_config->set_int(mod_ctx, g_settings.bloomHaloWidth, 220);
        svc_config->set_int(mod_ctx, g_settings.bloomHaloContribution, 100);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightRolloff, 120);
        svc_config->set_int(mod_ctx, g_settings.bloomHighlightLimit, 130);
        svc_config->set_int(mod_ctx, g_settings.bloomColourLayerStrength, 0);
        svc_config->set_bool(mod_ctx, g_settings.bloomPulseEnabled, false);
        break;
    default: break;
    }
}

void restore_bloom_profile(ModContext*, void*) {
    for (ConfigVarHandle handle : {g_settings.bloomCoreWidth, g_settings.bloomCoreContribution,
             g_settings.bloomInnerWidth, g_settings.bloomInnerContribution,
             g_settings.bloomOuterWidth, g_settings.bloomOuterContribution,
             g_settings.bloomHaloWidth, g_settings.bloomHaloContribution})
        svc_config->set_int(mod_ctx, handle, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomDiffusion, 0);
    svc_config->set_int(mod_ctx, g_settings.bloomPreset, 0);
}

void restore_better_bloom_defaults(ModContext*, void*) {
    svc_config->set_bool(mod_ctx, g_settings.bloomEnabled, true);
    set_bloom_profile(100, 100, 0, 100, 100, 0);
    restore_bloom_profile(mod_ctx, nullptr);
    svc_config->set_int(mod_ctx, g_settings.bloomThreshold, 50);
    svc_config->set_int(mod_ctx, g_settings.bloomThresholdSource, 0);
    svc_config->set_int(mod_ctx, g_settings.bloomOverrideThreshold, 100);
    svc_config->set_int(mod_ctx, g_settings.bloomThresholdNormalization, 50);
    svc_config->set_bool(mod_ctx, g_settings.bloomPulseEnabled, false);
    svc_config->set_int(mod_ctx, g_settings.bloomPulsePeriod, 4000);
    svc_config->set_int(mod_ctx, g_settings.bloomBlendMode, 0);
    svc_config->set_int(mod_ctx, g_settings.bloomResolution, 1);
}

ModResult build_better_bloom_group(
    ModContext*, UiElementHandle pane, void*, ModError*) {
    svc_ui->pane_add_section(mod_ctx, pane, "Presets");
    add_dropdown(pane, "Preset", "Apply a complete Better Bloom profile.",
        g_settings.bloomPreset, kBloomPresets);
    svc_ui->pane_add_section(mod_ctx, pane, "Bloom Options");
    add_toggle(pane, "Enable Better Bloom", "Enable the Better Bloom renderer for this mode.",
        g_settings.bloomEnabled);
    add_number(pane, "Intensity",
        "Scale the Better Bloom response after the original Bloom Brightness setting.",
        g_settings.betterBloomIntensity, 0, 20000, 5, "%");
    add_number(pane, "Size",
        "Scale the bloom radius. 100% preserves the area's authored value.",
        g_settings.bloomSize, 0, 4000, 5, "%");
    add_number(pane, "Soft Threshold", "Soften the bloom cutoff around bright pixels.",
        g_settings.bloomThreshold, 0, 667, 1, "%");
    svc_ui->pane_add_section(mod_ctx, pane, "Glow Profile");
    add_number(pane, "Diffusion",
        "Adjust how strongly the filtered glow spreads into the final bloom image.",
        g_settings.bloomDiffusion, 0, 4000, 5, "%");
    add_number(pane, "Core Width", "Tight central glow width.", g_settings.bloomCoreWidth, 0, 5000, 5, "%");
    add_number(pane, "Core Contribution", "Tight central glow strength.", g_settings.bloomCoreContribution, 0, 2000, 5, "%");
    add_number(pane, "Inner Glow Width", "Inner glow width.", g_settings.bloomInnerWidth, 0, 5000, 5, "%");
    add_number(pane, "Inner Glow Contribution", "Inner glow strength.", g_settings.bloomInnerContribution, 0, 2000, 5, "%");
    add_number(pane, "Outer Glow Width", "Outer glow width.", g_settings.bloomOuterWidth, 0, 5000, 5, "%");
    add_number(pane, "Outer Glow Contribution", "Outer glow strength.", g_settings.bloomOuterContribution, 0, 2000, 5, "%");
    add_number(pane, "Halo Width", "Wide halo width.", g_settings.bloomHaloWidth, 0, 5000, 5, "%");
    add_number(pane, "Halo Contribution", "Wide halo strength.", g_settings.bloomHaloContribution, 0, 2000, 5, "%");
    add_button(pane, "Restore Glow Profile Defaults", "Reset only the glow profile layers.",
        restore_bloom_profile);
    svc_ui->pane_add_section(mod_ctx, pane, "Bloom Response");
    add_dropdown(pane, "Gain Source", "Authored or fixed response gain.", g_settings.bloomGainSource, kBloomGainSources);
    add_number(pane, "Response Gain", "Bloom response gain.", g_settings.bloomResponseGain, 0, 4000, 5, "%");
    add_number(pane, "Highlight Rolloff", "Soften extreme highlights.", g_settings.bloomHighlightRolloff, 0, 400, 5, "%");
    add_toggle(pane, "Auto Compensation", "Compensate intensity when response changes.", g_settings.bloomAutoCompensation);
    add_number(pane, "Highlight Limit", "Limit highlight contribution; 0% disables it.", g_settings.bloomHighlightLimit, 0, 4000, 5, "%");
    add_number(pane, "Highlight Desaturation", "Desaturate the brightest bloom.", g_settings.bloomHighlightDesaturation, 0, 100, 1, "%");
    svc_ui->pane_add_section(mod_ctx, pane, "Colour");
    add_dropdown(pane, "Tint Source", "Authored or neutral tint source.", g_settings.bloomTintSource, kBloomTintSources);
    add_number(pane, "Saturation",
        "Adjust bloom colour saturation. 0% is monochrome and 100% preserves the authored colour.",
        g_settings.bloomSaturation, 0, 300, 5, "%");
    add_number(pane, "Tint Red", "Red tint multiplier.", g_settings.bloomTintRed, 0, 200, 1, "%");
    add_number(pane, "Tint Green", "Green tint multiplier.", g_settings.bloomTintGreen, 0, 200, 1, "%");
    add_number(pane, "Tint Blue", "Blue tint multiplier.", g_settings.bloomTintBlue, 0, 200, 1, "%");
    add_number(pane, "Colour Layer Strength", "Coloured bloom layer strength.", g_settings.bloomColourLayerStrength, 0, 1000, 5, "%");
    add_number(pane, "Scene Darkening", "Darken the scene beneath bloom.", g_settings.bloomSceneDarkening, 0, 100, 1, "%");
    svc_ui->pane_add_section(mod_ctx, pane, "Pulse Options");
    add_toggle(pane, "Enable Pulsing", "Enable frame-rate-independent pulsing.", g_settings.bloomPulseEnabled);
    add_dropdown(pane, "Pulse Activation", "Vanilla areas or everywhere.", g_settings.bloomPulseActivation, kBloomActivations);
    add_dropdown(pane, "Blend Mode",
        "Use the area's authored blend mode, or force Better Bloom's Additive or Screen blend.",
        g_settings.bloomBlendMode, kBloomBlendModes);
    add_number(pane, "Pulse Cycle",
        "Set the duration of one complete bloom pulse.",
        g_settings.bloomPulsePeriod, 100, 60000, 100, " ms");
    add_toggle(pane, "Irregular Timing", "Vary pulse timing.", g_settings.bloomPulseIrregular);
    add_number(pane, "Pulse Size Variation",
        "Adjust how much bloom size changes during the custom pulse.",
        g_settings.bloomPulseSize, 0, 100, 5, "%");
    add_number(pane, "Pulse Intensity Variation",
        "Adjust how much bloom intensity changes during the custom pulse.",
        g_settings.bloomPulseIntensity, 0, 100, 5, "%");
    add_number(pane, "Area Transition", "Pulse transition time between areas.", g_settings.bloomPulseTransition, 0, 10000, 100, " ms");
    svc_ui->pane_add_section(mod_ctx, pane, "Overrides");
    add_dropdown(pane, "Threshold Source", "Authored, custom, or normalized threshold.", g_settings.bloomThresholdSource, kBloomThresholdSources);
    add_number(pane, "Threshold Value", "Custom bloom threshold value.",
        g_settings.bloomOverrideThreshold, 0, 400, 1, "%");
    add_number(pane, "Threshold Normalization", "Threshold normalization amount.", g_settings.bloomThresholdNormalization, 0, 100, 1, "%");
    add_dropdown(pane, "Bloom Activation", "Vanilla areas or everywhere.", g_settings.bloomActivation, kBloomActivations);
    svc_ui->pane_add_section(mod_ctx, pane, "Performance");
    add_dropdown(pane, "Processing Resolution", "Full, half, or quarter resolution.", g_settings.bloomResolution, kBloomResolutions);
    add_button(pane, "Restore All Defaults", "Reset every Better Bloom setting.",
        restore_better_bloom_defaults);
    svc_ui->pane_add_text(mod_ctx, pane,
        "Defaults preserve the original area settings.",
        nullptr);
    return MOD_OK;
}

void better_bloom_window_closed(ModContext*, UiWindowHandle, void*) {
    g_betterBloomWindow = 0;
}

ModResult build_better_bloom_tab(ModContext*, UiWindowHandle, UiElementHandle left,
    UiElementHandle, void*, ModError* error) {
    return build_better_bloom_group(mod_ctx, left, nullptr, error);
}

void open_better_bloom_settings(ModContext*, void*) {
    if (g_betterBloomWindow != 0) return;
    UiTabDesc tabs[1] = {UI_TAB_DESC_INIT};
    tabs[0].title = "Better Bloom Settings";
    tabs[0].build = build_better_bloom_tab;
    UiWindowDesc window = UI_WINDOW_DESC_INIT;
    window.tabs = tabs;
    window.tab_count = 1;
    window.on_closed = better_bloom_window_closed;
    svc_ui->window_push(mod_ctx, &window, &g_betterBloomWindow);
}

void settings_window_closed(ModContext*, UiWindowHandle, void*) {
    commit_current_area_brightness_editor();
    g_areaEditorKey.clear();
    g_settingsWindow = 0;
    g_styleGroupControl = 0;
    g_chromaticAberrationControl = 0;
}

void open_settings_window(ModContext*, void*) {
    if (g_settingsWindow != 0) return;
    prepare_current_area_brightness_editor();
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

void get_dark_hour_fog_value(ModContext*, void*, UiControlValue* outValue) {
    if (outValue == nullptr) return;
    outValue->int_value = std::clamp<int64_t>(
        get_int(g_settings.darkHourFogStart, 0), 0, 400);
}

void adjust_dark_hour_fog_value(ModContext*, void*, const UiControlValue* value) {
    if (value != nullptr) {
        svc_config->set_int(mod_ctx, g_settings.darkHourFogStart,
            std::clamp<int64_t>(value->int_value, 0, 400));
    }
    native_face_tuner::open_dark_hour_fog();
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

int64_t current_area_brightness_percent() {
    sync_current_area_brightness_editor();
    if (!get_bool(g_settings.perAreaBrightness)) return 100;
    const std::string key = current_area_brightness_key();
    const auto it = g_areaBrightness.find(key);
    return it == g_areaBrightness.end() ? 100 : it->second;
}

ModResult register_settings(ModError*) {
    ModResult result = register_bool("twilight-visuals", false, g_settings.enabled);
    if (result != MOD_OK) return result;
    result = register_bool("visual-effects", true, g_settings.visualEffects);
    if (result != MOD_OK) return result;
    result = register_bool("twilight-camera-light", true, g_settings.twilightCameraLight);
    if (result != MOD_OK) return result;
    result = register_int("visual-style", 0, g_settings.style);
    if (result != MOD_OK) return result;
    result = register_int("brightness", 100, g_settings.brightness);
    if (result != MOD_OK) return result;
    result = register_bool("per-area-brightness", false, g_settings.perAreaBrightness);
    if (result != MOD_OK) return result;
    result = register_int("current-area-brightness", 100, g_settings.currentAreaBrightness);
    if (result != MOD_OK) return result;
    load_area_brightness();
    result = svc_config->subscribe(mod_ctx, g_settings.currentAreaBrightness,
        area_brightness_changed, nullptr, &g_areaBrightnessSubscription);
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
    result = register_int("better-bloom-intensity", 100, g_settings.betterBloomIntensity);
    if (result != MOD_OK) return result;
    result = register_int("bloom-size", 100, g_settings.bloomSize);
    if (result != MOD_OK) return result;
    result = register_int("bloom-threshold", 100, g_settings.bloomThreshold);
    if (result != MOD_OK) return result;
    result = register_int("bloom-diffusion", 100, g_settings.bloomDiffusion);
    if (result != MOD_OK) return result;
    result = register_int("bloom-saturation", 100, g_settings.bloomSaturation);
    if (result != MOD_OK) return result;
    result = register_int("bloom-blend-mode", 0, g_settings.bloomBlendMode);
    if (result != MOD_OK) return result;
    result = register_bool("bloom-pulse-enabled", false, g_settings.bloomPulseEnabled);
    if (result != MOD_OK) return result;
    result = register_int("bloom-pulse-period", 1500, g_settings.bloomPulsePeriod);
    if (result != MOD_OK) return result;
    result = register_int("bloom-pulse-size", 0, g_settings.bloomPulseSize);
    if (result != MOD_OK) return result;
    result = register_int("bloom-pulse-intensity", 0, g_settings.bloomPulseIntensity);
    if (result != MOD_OK) return result;
    result = register_int("bloom-core-width", 100, g_settings.bloomCoreWidth);
    if (result != MOD_OK) return result;
    result = register_int("bloom-core-contribution", 100, g_settings.bloomCoreContribution);
    if (result != MOD_OK) return result;
    result = register_int("bloom-inner-width", 100, g_settings.bloomInnerWidth);
    if (result != MOD_OK) return result;
    result = register_int("bloom-inner-contribution", 100, g_settings.bloomInnerContribution);
    if (result != MOD_OK) return result;
    result = register_int("bloom-outer-width", 100, g_settings.bloomOuterWidth);
    if (result != MOD_OK) return result;
    result = register_int("bloom-outer-contribution", 100, g_settings.bloomOuterContribution);
    if (result != MOD_OK) return result;
    result = register_int("bloom-halo-width", 100, g_settings.bloomHaloWidth);
    if (result != MOD_OK) return result;
    result = register_int("bloom-halo-contribution", 100, g_settings.bloomHaloContribution);
    if (result != MOD_OK) return result;
    result = register_int("bloom-gain-source", 0, g_settings.bloomGainSource);
    if (result != MOD_OK) return result;
    result = register_int("bloom-response-gain", 100, g_settings.bloomResponseGain);
    if (result != MOD_OK) return result;
    result = register_int("bloom-highlight-rolloff", 100, g_settings.bloomHighlightRolloff);
    if (result != MOD_OK) return result;
    result = register_bool("bloom-auto-compensation", true, g_settings.bloomAutoCompensation);
    if (result != MOD_OK) return result;
    result = register_int("bloom-highlight-limit", 0, g_settings.bloomHighlightLimit);
    if (result != MOD_OK) return result;
    result = register_int("bloom-highlight-desaturation", 0, g_settings.bloomHighlightDesaturation);
    if (result != MOD_OK) return result;
    result = register_int("bloom-tint-source", 0, g_settings.bloomTintSource);
    if (result != MOD_OK) return result;
    result = register_int("bloom-tint-red", 100, g_settings.bloomTintRed);
    if (result != MOD_OK) return result;
    result = register_int("bloom-tint-green", 100, g_settings.bloomTintGreen);
    if (result != MOD_OK) return result;
    result = register_int("bloom-tint-blue", 100, g_settings.bloomTintBlue);
    if (result != MOD_OK) return result;
    result = register_int("bloom-colour-layer-strength", 100, g_settings.bloomColourLayerStrength);
    if (result != MOD_OK) return result;
    result = register_int("bloom-scene-darkening", 0, g_settings.bloomSceneDarkening);
    if (result != MOD_OK) return result;
    result = register_int("bloom-pulse-activation", 0, g_settings.bloomPulseActivation);
    if (result != MOD_OK) return result;
    result = register_bool("bloom-pulse-irregular", false, g_settings.bloomPulseIrregular);
    if (result != MOD_OK) return result;
    result = register_int("bloom-pulse-transition", 500, g_settings.bloomPulseTransition);
    if (result != MOD_OK) return result;
    result = register_int("bloom-threshold-source", 0, g_settings.bloomThresholdSource);
    if (result != MOD_OK) return result;
    result = register_int("bloom-threshold-normalization", 50, g_settings.bloomThresholdNormalization);
    if (result != MOD_OK) return result;
    result = register_int("bloom-activation", 0, g_settings.bloomActivation);
    if (result != MOD_OK) return result;
    result = register_int("bloom-resolution", 1, g_settings.bloomResolution);
    if (result != MOD_OK) return result;
    result = register_bool("better-bloom-enabled", true, g_settings.bloomEnabled);
    if (result != MOD_OK) return result;
    result = register_int("better-bloom-preset", 0, g_settings.bloomPreset);
    if (result != MOD_OK) return result;
    result = register_int("bloom-override-threshold", 100, g_settings.bloomOverrideThreshold);
    if (result != MOD_OK) return result;
    result = svc_config->subscribe(mod_ctx, g_settings.bloomPreset,
        bloom_preset_changed, nullptr, nullptr);
    if (result != MOD_OK) return result;
    result = register_int("dark-hour-fog-start", 0, g_settings.darkHourFogStart);
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
    result = register_int("hotkey-gyro-captured-v3", 0, g_settings.hotkeyGyro);
    if (result != MOD_OK) return result;
    result = register_int("hotkey-bloom-captured-v3", 0, g_settings.hotkeyBloom);
    if (result != MOD_OK) return result;
    result = register_int("hotkey-textures-captured-v3", 0, g_settings.hotkeyTextures);
    if (result != MOD_OK) return result;
    result = register_bool("hotkey-gyro-enabled", true, g_settings.hotkeyGyroEnabled);
    if (result != MOD_OK) return result;
    result = register_bool("hotkey-textures-enabled", true, g_settings.hotkeyTexturesEnabled);
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
    if (g_betterBloomWindow != 0) {
        svc_ui->window_close(mod_ctx, g_betterBloomWindow);
        g_betterBloomWindow = 0;
    }
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
