#include "native_face_tuner.hpp"

#include "settings.hpp"

#include "dusk/ui/graphics_tuner.hpp"
#include "dusk/ui/ui.hpp"
#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"

#include <algorithm>
#include <memory>
#include <new>

extern const ConfigService* svc_config;
extern const HookService* svc_hook;

namespace twilight_visuals::native_face_tuner {
namespace {
using namespace dusk::ui;

constexpr auto kFaceOption = static_cast<GraphicsOption>(7);
constexpr auto kDarkHourFogOption = static_cast<GraphicsOption>(8);
using ConstructorFn = void (*)(GraphicsTuner*, GraphicsTunerProps);
using PushDocumentFn = Document* (*)(std::unique_ptr<Document>, bool, bool);
using RefreshFn = void (*)(SteppedCarousel*);
using TopDocumentFn = Document* (*)();

ConstructorFn gConstructor = nullptr;
PushDocumentFn gPushDocument = nullptr;
RefreshFn gRefresh = nullptr;
TopDocumentFn gTopDocument = nullptr;
bool gSettingHook = false;
bool gCarouselHook = false;

DEFINE_HOOK_SYMBOL("dusk::ui::GraphicsSetting::of",
    const GraphicsSetting*(GraphicsOption), GraphicsSettingOf);
DEFINE_HOOK_SYMBOL("dusk::ui::SteppedCarousel::apply",
    void(SteppedCarousel*, int), CarouselApply);

int read_face() {
    bool enabled = false;
    int64_t expression = 0;
    svc_config->get_bool(mod_ctx, settings().faceOverride, &enabled);
    svc_config->get_int(mod_ctx, settings().faceExpression, &expression);
    return enabled ? static_cast<int>(std::clamp<int64_t>(expression, 0, 162)) + 1 : 0;
}

void write_face(int value) {
    value = std::clamp(value, 0, 163);
    svc_config->set_bool(mod_ctx, settings().faceOverride, value != 0);
    if (value != 0) svc_config->set_int(mod_ctx, settings().faceExpression, value - 1);
}

Rml::String label_face(int value) {
    static constexpr const char* names[] = {
        "Automatic (Game Controlled)",
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
    static_assert(std::size(names) == 164);
    return names[std::clamp(value, 0, 163)];
}

bool modified_face() { return read_face() != 0; }

int read_dark_hour_fog() {
    int64_t value = 0;
    svc_config->get_int(mod_ctx, settings().darkHourFogStart, &value);
    return static_cast<int>(std::clamp<int64_t>(value, 0, 200));
}

void write_dark_hour_fog(int value) {
    svc_config->set_int(mod_ctx, settings().darkHourFogStart, std::clamp(value, 0, 200));
}

Rml::String label_percent(int value) { return std::to_string(std::clamp(value, 0, 200)) + "%"; }

bool modified_dark_hour_fog() { return read_dark_hour_fog() != 0; }

GraphicsSetting gFaceSetting{
    .min = 0, .max = 163, .defaultValue = 0, .step = 1,
    .watchesRenderSize = false, .read = read_face, .write = write_face,
    .label = label_face, .cvarName = []() -> const char* { return nullptr; },
    .isModified = modified_face,
};

HookAction setting_pre(ModContext*, void* args, void* retval, void*) {
    const auto option = mods::arg<GraphicsOption>(args, 0);
    if (option == kFaceOption) {
        *static_cast<const GraphicsSetting**>(retval) = &gFaceSetting;
        return HOOK_SKIP_ORIGINAL;
    }
    if (option == kDarkHourFogOption) {
        static const GraphicsSetting setting{
            .min = 0,
            .max = 200,
            .defaultValue = 0,
            .step = 5,
            .watchesRenderSize = false,
            .read = read_dark_hour_fog,
            .write = write_dark_hour_fog,
            .label = label_percent,
            .cvarName = []() -> const char* {
                return "mod.dev_twilitrealm_twilight__visuals__vanilla.dark-hour-fog-start";
            },
            .isModified = modified_dark_hour_fog,
        };
        *static_cast<const GraphicsSetting**>(retval) = &setting;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void carousel_post(ModContext*, void* args, void*, void*) {
    if (gRefresh != nullptr) gRefresh(mods::arg<SteppedCarousel*>(args, 0));
}

bool resolve(const char* name, void** out) {
    return svc_hook->resolve(mod_ctx, name, out, nullptr) == MOD_OK && *out != nullptr;
}
}

ModResult initialize() {
#if defined(__ANDROID__)
    // Dusklight's Android shell does not export the private native graphics
    // tuner symbols used by this optional desktop integration. Avoid installing
    // partial hooks before returning unavailable; the rest of the mod remains
    // fully usable through its public settings UI.
    return MOD_UNAVAILABLE;
#endif
    ModResult result = mods::hook::add_pre<GraphicsSettingOf>(setting_pre);
    if (result != MOD_OK) return result;
    gSettingHook = true;
    result = mods::hook::add_post<CarouselApply>(carousel_post);
    if (result != MOD_OK) return result;
    gCarouselHook = true;

    void* fn = nullptr;
#if defined(__APPLE__)
    if (!resolve("_ZN4dusk2ui13GraphicsTunerC1ENS0_18GraphicsTunerPropsE", &fn))
        return MOD_UNAVAILABLE;
#else
    if (!resolve("dusk::ui::GraphicsTuner::GraphicsTuner", &fn)) return MOD_UNAVAILABLE;
#endif
    gConstructor = reinterpret_cast<ConstructorFn>(fn);
    fn = nullptr;
#if defined(__APPLE__)
    if (!resolve(
            "_ZN4dusk2ui13push_documentENSt3__110unique_ptrINS0_8DocumentENS1_14default_deleteIS3_EEEEbb",
            &fn))
        return MOD_UNAVAILABLE;
#else
    if (!resolve("dusk::ui::push_document", &fn)) return MOD_UNAVAILABLE;
#endif
    gPushDocument = reinterpret_cast<PushDocumentFn>(fn);
    fn = nullptr;
#if defined(__APPLE__)
    if (!resolve("_ZN4dusk2ui15SteppedCarousel7refreshEv", &fn)) return MOD_UNAVAILABLE;
#else
    if (!resolve("dusk::ui::SteppedCarousel::refresh", &fn)) return MOD_UNAVAILABLE;
#endif
    gRefresh = reinterpret_cast<RefreshFn>(fn);
    fn = nullptr;
#if defined(__APPLE__)
    if (!resolve("_ZN4dusk2ui12top_documentEv", &fn)) return MOD_UNAVAILABLE;
#else
    if (!resolve("dusk::ui::top_document", &fn)) return MOD_UNAVAILABLE;
#endif
    gTopDocument = reinterpret_cast<TopDocumentFn>(fn);
    return MOD_OK;
}

void open_tuner(GraphicsOption option, const char* title, const char* helpText) {
    if (gConstructor == nullptr || gPushDocument == nullptr || gTopDocument == nullptr) return;
    void* memory = ::operator new(sizeof(GraphicsTuner));
    auto* tuner = static_cast<GraphicsTuner*>(memory);
    gConstructor(tuner, GraphicsTunerProps{
        .option = option,
        .title = title,
        .helpText = helpText,
    });
    std::unique_ptr<Document> document(tuner);
    if (auto* current = gTopDocument()) current->cover();
    gPushDocument(std::move(document), true, false);
}

void open() {
    open_tuner(kFaceOption, "Facial Expression",
        "Override Link's facial expression. Automatic returns control to the game. "
        "Human and wolf entries apply to their matching form.");
}

void open_dark_hour_fog() {
    open_tuner(kDarkHourFogOption, "Dark Hour Foreground Visibility",
        "Adjust how far the Dark Hour foreground remains visible around Link. Higher values "
        "push the terrain fog farther away while preserving the distant darkness.");
}

void shutdown() {
    if (gCarouselHook) mods::hook::uninstall<CarouselApply>();
    if (gSettingHook) mods::hook::uninstall<GraphicsSettingOf>();
    gCarouselHook = false;
    gSettingHook = false;
    gConstructor = nullptr;
    gPushDocument = nullptr;
    gRefresh = nullptr;
    gTopDocument = nullptr;
}
}
