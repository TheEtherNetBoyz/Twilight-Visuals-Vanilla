#include "music.hpp"
#include "hook_api.hpp"
#include "runtime.hpp"
#include "TwilightMusicFade.h"
#include "JSystem/JAudio2/JASAiCtrl.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "Z2AudioLib/Z2SceneMgr.h"
#include "Z2AudioLib/Z2StatusMgr.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_Reset.h"
#include "m_Do/m_Do_audio.h"
#include "mods/svc/hook.hpp"
#include "Z2AudioLib/Z2Param.h"
#include "Z2AudioLib/Z2SeqMgr.h"
#include "mods/svc/log.h"
#include "platform.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <cstddef>
#include <cstdio>
#include <string>
#define DR_MP3_IMPLEMENTATION
#include "third_party/dr_mp3.h"

namespace twilight_visuals::music {
bool is_boss_bgm(u32 id);
void update_from_manager(Z2SeqMgr* manager);
namespace {
using dusk::audio::TwilightMusicFade;

bool init_mp3(drmp3* decoder, const std::filesystem::path& path) {
#if defined(_WIN32)
    return drmp3_init_file_w(decoder, path.c_str(), nullptr) != 0;
#else
    const auto utf8_path = path.u8string();
    return drmp3_init_file(decoder,
        reinterpret_cast<const char*>(utf8_path.c_str()), nullptr) != 0;
#endif
}

#if defined(__APPLE__)
std::filesystem::path mac_user_music_directory() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return {};
    return std::filesystem::path(home) /
        "Library/Application Support/TwilitRealm/Dusklight/Twilight Visuals/custom music";
}
#endif

std::filesystem::path music_file(const std::filesystem::path& executableDirectory,
                                 const char* filename) {
#if defined(__APPLE__)
    const auto userDirectory = mac_user_music_directory();
    if (!userDirectory.empty()) {
        std::error_code error;
        const auto userFile = userDirectory / filename;
        if (std::filesystem::is_regular_file(userFile, error)) return userFile;
    }
#endif
    return executableDirectory / filename;
}

Z2SceneMgr* LiveSceneManager = nullptr;
bool sceneSelectionKnown = false;
bool previousReplacementSelection = false;

void refresh_scene_music_selection(bool replacementWanted, Z2SeqMgr* sequence,
                                   Z2StatusMgr* status) {
    Z2SceneMgr* scene = LiveSceneManager ? LiveSceneManager : Z2GetSceneMgr();
    // Menus can retain the previous gameplay stage name. Never try to restore
    // authored gameplay audio from that stale name while title/file select is
    // active; vanilla owns the menu transition and its audio state.
    if (!replacementWanted && title_or_file_select_active()) return;
    if (!replacementWanted && dComIfGp_getPlayer(0) == nullptr &&
        !dComIfGp_isEnableNextStage()) return;
    // The vanilla macOS build does not expose a status-manager singleton at
    // this hook point. update_from_manager already treats that as demo 0; do
    // the same here instead of rejecting every scene-selection refresh.
    const bool missingState = !sequence || !scene;
    const bool sceneNotReady = scene != nullptr && !scene->isSceneExist();
    const bool nextStage = dComIfGp_isEnableNextStage();
    const u8 demoStatus = status != nullptr ? status->getDemoStatus() : 0;
    const bool eventRunning = dComIfGp_event_runCheck();
    if (missingState || sceneNotReady || nextStage || demoStatus != 0 || eventRunning) {
        return;
    }

    // During a room transition the sequence manager can continue reporting the
    // outgoing track while it fades. The scene manager's requested ID is the
    // authoritative choice for the area that is being loaded.
    const bool palaceRequested = static_cast<u32>(scene->BGM_ID) == Z2BGM_DUNGEON_LV8;
    const bool selectionChanged = !sceneSelectionKnown ||
                                  replacementWanted != previousReplacementSelection;
    if (!selectionChanged && (!replacementWanted || palaceRequested)) return;
    if (replacementWanted && palaceRequested) {
        sceneSelectionKnown = true;
        previousReplacementSelection = true;
        return;
    }

    const char* stage = dComIfGp_getStartStageName();
    if (!stage || !*stage) return;
    const s8 room = dComIfGp_roomControl_getStayNo();
    const s8 layer = dComIfG_play_c::getLayerNo_common(
        stage, room, dComIfGp_getStartStageLayer());

    if (replacementWanted) {
        // The vanilla macOS build can have the legacy Z2SoundMgr singleton
        // unset even though the audio interface is alive. Do not touch the
        // scene or stop the current BGM unless the supported audio interface
        // is available; the next framework pass can retry this refresh.
        Z2AudioMgr* audio = Z2GetAudioMgr();
        if (audio == nullptr) {
            svc_log->warn(mod_ctx,
                "SceneMusicRefresh skipped: audio interface unavailable");
            return;
        }

        // Do not depend on the provider being consulted during an in-place
        // settings refresh. Some vanilla builds keep the existing scene
        // selection and bypass that callback. Program the same native Palace
        // sequence and wave bank that Palace of Twilight itself requests.
        scene->sceneChange(JAISoundID(Z2BGM_DUNGEON_LV8),
            scene->requestSeWave_1, scene->requestSeWave_2,
            0x28, 0, scene->requestDemoWave, false);

        // sceneBgmStart() is not safe on the vanilla macOS build because its
        // status-manager singleton is unavailable there. Recreate the small
        // native start sequence directly after loading the requested wave;
        // leaving sceneExist false here makes every non-Palace area go silent.
        scene->load1stWait = 0;
        scene->_load1stWaveInner_2();
        sequence->bgmStop(0, 0);
        audio->startSound(JAISoundID(Z2BGM_DUNGEON_LV8),
            sequence->getMainBgmHandle(), nullptr);
        sequence->mMainBgmMaster.forceIn();
        sequence->mSceneBgm.forceIn();
        sequence->mAllBgmMaster.forceIn();
        // The vanilla Palace sequence uses room number as its layer selector:
        // nonzero room values select the interior arrangement. Keep status 0
        // so the exterior Palace arrangement is used everywhere.
        // A save can leave the cached status at 0 while the newly reused
        // sequence handle still has combat child-track volumes. The vanilla
        // implementation returns early for status 0 in that case, so mark
        // the status as uninitialized before applying the exterior layout.
        sequence->mBgmStatus = 0xff;
        sequence->changeBgmStatus(0);
        scene->field_0x1a = false;
        scene->setSceneExist(true);
    } else {
        // Disabling the replacement must restore the room's authored BGM and
        // wave banks, so run the complete vanilla scene lookup in that case.
        mDoAud_setSceneName(stage, room, layer);
    }
    scene->timer = 0;
    if (!replacementWanted) {
        scene->setSceneExist(false);
        if (scene->load1stWait == 0)
            scene->_load1stWaveInner_1();
        mDoAud_zelAudio_c::onBgmSet();
    }

    sceneSelectionKnown = true;
    previousReplacementSelection = replacementWanted;

    char message[192]{};
    std::snprintf(message, sizeof(message),
        "SceneMusicRefresh wanted=%d bgm=%08x wave1=%u room=%d layer=%d",
        replacementWanted ? 1 : 0,
        static_cast<unsigned>(static_cast<u32>(scene->BGM_ID)),
        static_cast<unsigned>(scene->requestBgmWave_1),
        static_cast<int>(room), static_cast<int>(layer));
    svc_log->info(mod_ctx, message);
}

struct Track {
    drmp3 decoder{};
    std::filesystem::path path;
    bool available = false;
    bool playbackStarted = false;
    float audibleGain = 0;
    std::array<float, 4096> decoded{};
    size_t frameIndex = 0, frameCount = 0;
    std::array<float, 2> a{}, b{};
    bool primed = false;
    double position = 0;

    void close() {
        if (available) drmp3_uninit(&decoder);
        decoder = {};
        available = playbackStarted = primed = false;
        audibleGain = 0;
        frameIndex = frameCount = 0;
        position = 0;
    }
    void open(const std::filesystem::path& file) {
        close();
        path = file;
        available = init_mp3(&decoder, path);
        if (available && (decoder.channels == 0 || decoder.channels > 2 || decoder.sampleRate == 0))
            close();
        const auto name = path.u8string();
        const std::string message = std::string(available ? "Streaming music: " : "Music unavailable (missing or unsupported): ") +
            std::string(reinterpret_cast<const char*>(name.data()), name.size());
        svc_log->write(mod_ctx, available ? LOG_LEVEL_INFO : LOG_LEVEL_WARN, message.c_str());
    }
    bool rewind() {
        if (!available) return false;
        if (!drmp3_seek_to_pcm_frame(&decoder, 0)) {
            drmp3_uninit(&decoder);
            decoder = {};
            available = init_mp3(&decoder, path);
        }
        frameIndex = frameCount = 0;
        return available;
    }
    bool next(std::array<float, 2>& sample) {
        if (frameIndex == frameCount) {
            frameIndex = 0;
            frameCount = static_cast<size_t>(drmp3_read_pcm_frames_f32(
                &decoder, decoded.size() / decoder.channels, decoded.data()));
            if (!frameCount) {
                if (!rewind()) return false;
                frameCount = static_cast<size_t>(drmp3_read_pcm_frames_f32(
                    &decoder, decoded.size() / decoder.channels, decoded.data()));
                if (!frameCount) return false;
            }
        }
        const size_t index = frameIndex++ * decoder.channels;
        sample = {decoded[index], decoded[index + (decoder.channels == 2 ? 1 : 0)]};
        return true;
    }
    void setGain(float target, float elapsed, bool smoothReduction = false,
                 bool rewindWhenSilent = false, bool immediate = false) {
        if (!available) return;
        if (target > 0) playbackStarted = true;
        const float step = std::clamp(elapsed, 0.0f, 0.05f);
        audibleGain = immediate ? target : smoothReduction
            ? audibleGain + std::clamp(target - audibleGain, -step, step)
            : std::min(target, audibleGain + step);
        if (rewindWhenSilent && target <= 0 && audibleGain <= 0.0001f && playbackStarted) {
            rewind();
            playbackStarted = primed = false;
            position = 0;
        }
    }
    void mix(float* output, u32 frames, u32 rate, float calibration, float volume,
             bool pauseWhenSilent = false) {
        if (!available || !playbackStarted || !rate ||
            (pauseWhenSilent && audibleGain <= 0.0001f)) return;
        if (!primed) {
            if (!next(a) || !next(b)) return;
            primed = true;
        }
        const double step = static_cast<double>(decoder.sampleRate) / rate;
        const float gain = audibleGain * calibration * volume * Z2Param::VOL_BGM_DEFAULT;
        for (u32 i = 0; i < frames; ++i) {
            for (u32 channel = 0; channel < 2; ++channel)
                output[2 * i + channel] +=
                    (a[channel] + (b[channel] - a[channel]) * static_cast<float>(position)) * gain;
            position += step;
            while (position >= 1.0) {
                a = b;
                if (!next(b)) return;
                position -= 1.0;
            }
        }
    }
};
Track AstralMp3Ambient, AstralMp3Combat, DarkHourAmbient, DarkHourCombat, MasterOfShadow;
std::atomic<float> TwilightMusicVolume{1};
std::atomic<float> PalaceGain{1}, BattleGain{1}, BossGain{1};
std::atomic<bool> NativeBgmMute{false};
std::atomic<u32> BossNativeMain{0xffffffff}, BossNativeSub{0xffffffff};
bool registered = false;
bool mixHookInstalled = false;
bool sceneHookInstalled = false;
bool sceneNameHookInstalled = false;
bool frameworkHookInstalled = false;
bool palaceStatusHookInstalled = false;
JASDriver::MixCallback NativeMixCallback = nullptr;
std::array<s16, 8192> CompositeMix{};
std::atomic<bool> sceneStartPending{true};
std::atomic<u64> customMixCalls{0};
std::atomic<float> customMixPeak{0.0f};
float ambientGainLatch = 1.0f;
std::atomic<bool> customOwnership{false};
std::atomic<bool> explicitlySuspended{false};
TwilightMusicFade fade, encounterFade, bossFade;
std::chrono::steady_clock::time_point lastTick{};
void update_sequence(bool replacementScene, bool eligible, int musicMode,
                     float gain, bool battleScope, bool battleActive, float battleVolume,
                     bool bossActive, float bossVolume, u32 bossMain, u32 bossSub) {
    const auto tick = std::chrono::steady_clock::now();
    const float elapsed = lastTick.time_since_epoch().count() == 0 ? 0.0f :
        std::chrono::duration<float>(tick - lastTick).count();
    lastTick = tick;
    const bool astral = musicMode == 1;
    const bool darkHour = musicMode == 2;
    const bool ready = AstralMp3Ambient.available;
    const bool combatReady = AstralMp3Combat.available;
    const bool darkHourReady = DarkHourAmbient.available;
    const bool darkHourCombatReady = DarkHourCombat.available;
    const bool bossReady = MasterOfShadow.available;
    const bool selectionScope = replacementScene || battleScope;
    const bool customSelected = astral || darkHour;
    const bool selectionReady = astral ? ready : (darkHour && darkHourReady);
    const bool currentTrackReady = battleActive ? (astral ? combatReady : darkHour && darkHourCombatReady) : selectionReady;
    const bool sceneStart = sceneStartPending.load() && replacementScene;
    // Keep every native scene score silent while a visual preset owns music. This remains
    // latched through scene teardown, so destination music cannot leak before its provider
    // callback and placeholder sequence have finished loading.
    NativeBgmMute.store(customSelected && selectionReady && replacementScene);
    // Silence the placeholder before it becomes audible, but do not start the
    // decoder until the native scene is ready to play music.
    fade.select(customSelected && currentTrackReady, selectionScope, elapsed, sceneStart);
    // A prepared stream remains alive and advances silently after its style is
    // deselected. Track identity must therefore gate every audible path; ready
    // alone does not mean that track is currently selected.
    const bool replaceAstralBattle = astral && battleScope && combatReady;
    const bool replaceDarkHourBattle = darkHour && battleScope && darkHourCombatReady;
    const bool replaceBattle = replaceAstralBattle || replaceDarkHourBattle;
    const bool enteringCombat = battleActive && replaceBattle;
    // Give the combat outro and exploration re-entry about 1.5 seconds each.
    // Keep combat entry/Palace selection at their existing speed, and preserve
    // the exclusive handoff so the two Astral tracks never play over each other.
    if (sceneStart) encounterFade.position = enteringCombat ? 1.0f : 0.0f;
    else encounterFade.update(enteringCombat, elapsed, enteringCombat ? 2.0f : 3.0f);
    bossFade.update(bossActive && bossReady, elapsed, 1.5f);
    if (bossActive) {
        BossNativeMain.store(bossMain);
        BossNativeSub.store(bossSub);
    }
    // Keep the replacement Palace sequence silent through interruptions and
    // AST preparation. Native Palace areas are outside replacementScene.
    PalaceGain.store(replacementScene && currentTrackReady ? fade.palace() : 1.0f);
    // Gate all ordinary battle sequences at their native channel output,
    // including detached fade-out tails. Never tag boss/miniboss themes.
    BattleGain.store(replaceBattle ? fade.palace() : 1.0f);
    BossGain.store(bossReady ? bossFade.palace() : 1.0f);
    // Zero volume is deliberately NOT stop or pause. The native loop and its
    // sample position keep advancing through battles, menus and track changes.
    const float targetGain = astral && eligible && ready && !bossActive &&
        (!battleActive || replaceAstralBattle) ?
        std::clamp(gain, 0.0f, 1.0f) * fade.astral() * encounterFade.palace() : 0.0f;
    // Gentle re-entry, immediate reductions: never let a fade-out trail cross
    // the exclusive handoff into Palace or protected music.
    AstralMp3Ambient.setGain(targetGain, elapsed, false, false, sceneStart);
    AstralMp3Combat.setGain(astral && replaceAstralBattle && !bossActive ? std::clamp(battleVolume, 0.0f, 1.0f) *
        fade.astral() * encounterFade.astral() : 0.0f, elapsed, true, true, sceneStart);
    DarkHourAmbient.setGain(darkHour && eligible && darkHourReady && !bossActive ? std::clamp(gain, 0.0f, 1.0f) *
        fade.astral() * encounterFade.palace() : 0.0f, elapsed, false, false, sceneStart);
    DarkHourCombat.setGain(replaceDarkHourBattle && battleActive && !bossActive ?
        std::clamp(battleVolume, 0.0f, 1.0f) * fade.astral() * encounterFade.astral() : 0.0f,
        elapsed, true, false, sceneStart);
    MasterOfShadow.setGain(bossActive && bossReady ?
        std::clamp(bossVolume, 0.0f, 1.0f) * bossFade.astral() : 0.0f,
        elapsed, true, !bossActive, sceneStart);
    if (sceneStart && eligible && gain > 0.0f) sceneStartPending.store(false);
}


void mix(float* output, u32 frames, u32 rate) {
    const float volume = TwilightMusicVolume.load();
    AstralMp3Ambient.mix(output, frames, rate, 0.85f, volume);
    AstralMp3Combat.mix(output, frames, rate, 0.85f, volume);
    DarkHourAmbient.mix(output, frames, rate, 0.65f, volume);
    // Preserve the combat track's decoder position between encounters. It advances during the
    // outro fade, pauses once silent, and resumes from that point on the next battle.
    DarkHourCombat.mix(output, frames, rate, 0.65f, volume, true);
    // Mix the replacement separately, then fit it around every existing native sample. This
    // keeps game voices and effects bit-for-bit intact and prevents the MP3 from clipping over
    // them without applying a buffer-wide duck or delaying the start of the boss track.
    std::array<float, 4096> bossMix{};
    u32 offset = 0;
    while (offset < frames) {
        const u32 chunk = std::min<u32>(frames - offset, bossMix.size() / 2);
        std::fill_n(bossMix.data(), chunk * 2, 0.0f);
        // The native boss sequence is muted below, so the signal already in
        // output is game SFX/voices/ambience. Side-chain only the replacement
        // music around that signal; never attenuate or overwrite native audio.
        float nativePeak = 0.0f;
        for (u32 i = 0; i < chunk * 2; ++i) {
            nativePeak = std::max(nativePeak, std::abs(output[offset * 2 + i]));
        }
        const float sfxDuck = std::clamp(1.0f - nativePeak * 1.25f, 0.28f, 1.0f);
        MasterOfShadow.mix(bossMix.data(), chunk, rate, 0.75f * sfxDuck, volume, true);
        for (u32 i = 0; i < chunk * 2; ++i) {
            const u32 outputIndex = offset * 2 + i;
            const float native = output[outputIndex];
            const float room = std::max(0.0f, 1.0f - std::abs(native));
            output[outputIndex] = native + std::clamp(bossMix[i], -room, room);
        }
        offset += chunk;
    }
}
float channel_gain(u32 channel) {
    // JAudio sequence IDs occupy the 0x01000000 range. Permit short fanfares while muting
    // scene/event music; sound effects and voices use separate SE buses and are untouched.
    const bool fanfare = channel == Z2BGM_ITEM_GET || channel == Z2BGM_ITEM_GET_MINI ||
        channel == Z2BGM_OPEN_BOX || channel == Z2BGM_ITEM_GET_ME ||
        channel == Z2BGM_HEART_GET || channel == Z2BGM_FISHING_GET1 ||
        channel == Z2BGM_FISHING_GET2 || channel == Z2BGM_FISHING_GET3 ||
        channel == Z2BGM_ITEM_GET_INSECT || channel == Z2BGM_ITEM_GET_SMELL ||
        channel == Z2BGM_ITEM_GET_POU;
    if (NativeBgmMute.load() && (channel & 0xff000000u) == 0x01000000u && !fanfare)
        return 0.0f;
    if (channel == Z2BGM_DUNGEON_LV8) return PalaceGain.load();
    if (channel == Z2BGM_BATTLE_NORMAL || channel == Z2BGM_BATTLE_TWILIGHT) return BattleGain.load();
    // Mute the native boss score while the streamed replacement fades in.
    // Ordinary action SFX use the SE buses and are deliberately unaffected.
    // 0xffffffff means "no sequence" and is also the default tag carried by
    // many non-BGM tracks. Never compare that sentinel as a real boss ID or it
    // will silence Link, enemy, and item sounds along with the music.
    const u32 bossMain = BossNativeMain.load();
    const u32 bossSub = BossNativeSub.load();
    const bool savedBoss = (bossMain != 0xffffffff && channel == bossMain) ||
                           (bossSub != 0xffffffff && channel == bossSub);
    if (is_boss_bgm(channel) || savedBoss) return BossGain.load();
    return 1.0f;
}

static s16* composite_mix(s32 frames) {
    customMixCalls.fetch_add(1, std::memory_order_relaxed);
    if (frames <= 0 || static_cast<size_t>(frames) * 2 > CompositeMix.size()) return nullptr;
    s16* native = NativeMixCallback ? NativeMixCallback(frames) : nullptr;
    std::array<float, 8192> custom{};
    mix(custom.data(), static_cast<u32>(frames), 32000);
    float peak = 0.0f;
    for (s32 i = 0; i < frames * 2; ++i) {
        peak = std::max(peak, std::abs(custom[i]));
        const float base = native ? native[i] / 32768.0f : 0.0f;
        CompositeMix[i] = static_cast<s16>(std::clamp(base + custom[i], -1.0f, 1.0f) * 32767.0f);
    }
    customMixPeak.store(peak, std::memory_order_relaxed);
    return CompositeMix.data();
}

DEFINE_HOOK_SYMBOL("JASDriver::registerMixCallback",
    void(JASDriver::MixCallback, JASMixMode), RegisterMixCallback);
DEFINE_HOOK_SYMBOL("Z2SceneMgr::sceneChange",
    void(Z2SceneMgr*, JAISoundID, u8, u8, u8, u8, u8, bool), SceneChange);
DEFINE_HOOK_SYMBOL("Z2SceneMgr::setSceneName",
    void(Z2SceneMgr*, char*, s32, s32), SetSceneName);
DEFINE_HOOK_SYMBOL("Z2SeqMgr::changeBgmStatus", void(Z2SeqMgr*, s32), ChangeBgmStatus);
DEFINE_HOOK_SYMBOL("Z2SeqMgr::processBgmFramework", void(Z2SeqMgr*), ProcessBgmFramework);

HookAction register_mix_pre(ModContext*, void* args, void*, void*) {
    auto& callback = mods::arg_ref<JASDriver::MixCallback>(args, 0);
    auto& mode = mods::arg_ref<JASMixMode>(args, 1);
    if (callback != &composite_mix) NativeMixCallback = callback;
    callback = &composite_mix;
    mode = MIX_MODE_INTERLEAVE;
    return HOOK_CONTINUE;
}

// Dusklight's PC mixer consumes these exported vanilla JAS globals directly.
// Keep our composite callback installed even if a movie temporarily replaces or
// clears the callback; the replaced callback is chained by composite_mix.
void bind_vanilla_mixer() {
    const auto callback = JASDriver::extMixCallback;
    if (callback != &composite_mix) {
        NativeMixCallback = callback;
        JASDriver::extMixCallback = &composite_mix;
    }
    JASDriver::sMixMode = MIX_MODE_INTERLEAVE;
}

bool fanfare(u32 id) {
    switch (id) {
    case Z2BGM_ITEM_GET: case Z2BGM_ITEM_GET_MINI: case Z2BGM_OPEN_BOX:
    case Z2BGM_ITEM_GET_ME: case Z2BGM_HEART_GET: case Z2BGM_FISHING_GET1:
    case Z2BGM_FISHING_GET2: case Z2BGM_FISHING_GET3:
    case Z2BGM_ITEM_GET_INSECT: case Z2BGM_ITEM_GET_SMELL: case Z2BGM_ITEM_GET_POU:
        return true;
    default: return false;
    }
}

void apply_native_gains(Z2SeqMgr* p) {
    if (!p) return;
    const u32 main = p->getMainBgmID();
    const u32 sub = p->getSubBgmID();
    const bool globalPalace =
        (runtime_settings().style == Style::Normal ||
         runtime_settings().style == Style::BlackAndWhite) &&
        main == Z2BGM_DUNGEON_LV8;
    float mainGain = 1.0f;
    float subGain = 1.0f;
    if (NativeBgmMute.load() && main != 0xffffffffu && !fanfare(main)) mainGain = 0.0f;
    if (NativeBgmMute.load() && sub != 0xffffffffu && !fanfare(sub)) subGain = 0.0f;
    if (main == Z2BGM_DUNGEON_LV8) mainGain = PalaceGain.load();
    if (sub == Z2BGM_BATTLE_NORMAL || sub == Z2BGM_BATTLE_TWILIGHT)
        subGain = globalPalace ? 0.0f : BattleGain.load();
    if (is_boss_bgm(main)) mainGain = BossGain.load();
    if (is_boss_bgm(sub)) subGain = BossGain.load();
    if (p->mMainBgmHandle) p->mMainBgmHandle->getAuxiliary().moveVolume(mainGain, 0);
    if (p->mSubBgmHandle) p->mSubBgmHandle->getAuxiliary().moveVolume(subGain, 0);
}

void framework_post(ModContext*, void* args, void*, void*) {
    // Use the live `this` pointer from vanilla's sequence framework. The
    // exported singleton accessor is not populated for mods in this build.
    update_from_manager(mods::arg<Z2SeqMgr*>(args, 0));
}

HookAction scene_change_pre(ModContext*, void* args, void*, void*) {
    auto* scene = mods::arg<Z2SceneMgr*>(args, 0);
    LiveSceneManager = scene;
    auto& bgm = mods::arg_ref<JAISoundID>(args, 1);
    auto& wave1 = mods::arg_ref<u8>(args, 4);
    auto& wave2 = mods::arg_ref<u8>(args, 5);
    const u8 demo = mods::arg<u8>(args, 6);
    const char* stage = dComIfGp_getStartStageName();
    bool streams = false, field = false;
    s32 status = -1;
    u32 id = static_cast<u32>(bgm);
    const bool replaced = provide_scene_music(stage, dComIfGp_roomControl_getStayNo(),
            dComIfGp_getStartStageLayer(), scene ? scene->getCurrentSceneNum() : -1,
            scene && scene->isInDarkness(), demo, &id, &wave1, &wave2,
            &streams, &field, &status);
    // Suppress destination BGM before its first audio frame while a load/void
    // transition is carrying custom ownership across the missing-player gap.
    // Normal and Black & White use the native Palace sequence, so they need
    // the same handoff protection or the authored destination track leaks for
    // a moment before the Palace request is processed.
    const bool globalPalace = runtime_settings().style == Style::Normal ||
                              runtime_settings().style == Style::BlackAndWhite;
    if (customOwnership.load(std::memory_order_relaxed) ||
        (replaced && globalPalace)) {
        NativeBgmMute.store(true, std::memory_order_relaxed);
    } else if (globalPalace) {
        NativeBgmMute.store(false, std::memory_order_relaxed);
    }
    if (replaced) {
        bgm = JAISoundID(id);
        sceneStartPending.store(true);
    }
    return HOOK_CONTINUE;
}

void scene_name_post(ModContext*, void* args, void*, void*) {
    auto* scene = mods::arg<Z2SceneMgr*>(args, 0);
    if (!scene) return;
    LiveSceneManager = scene;

    const char* stage = mods::arg<char*>(args, 1);
    const s32 room = mods::arg<s32>(args, 2);
    const s32 layer = mods::arg<s32>(args, 3);
    u32 id = static_cast<u32>(scene->BGM_ID);
    u8 wave1 = scene->requestBgmWave_1;
    u8 wave2 = scene->requestBgmWave_2;
    bool streams = false, field = false;
    s32 status = -1;
    const bool replaced = provide_scene_music(stage, room, layer, scene->getCurrentSceneNum(),
            scene->isInDarkness(), scene->requestDemoWave, &id, &wave1, &wave2,
            &streams, &field, &status);
    if (!replaced) {
        if (runtime_settings().style == Style::Normal ||
            runtime_settings().style == Style::BlackAndWhite) {
            NativeBgmMute.store(false, std::memory_order_relaxed);
        }
        return;
    }

    const bool globalPalace = runtime_settings().style == Style::Normal ||
                              runtime_settings().style == Style::BlackAndWhite;
    if (globalPalace) NativeBgmMute.store(true, std::memory_order_relaxed);

    // setSceneName has now finished vanilla's complete stage lookup. Apply the
    // replacement here, where the live scene object and wave requests are
    // authoritative, rather than relying solely on a sceneChange pre-hook
    // that is unavailable in some vanilla Dusklight builds.
    if (static_cast<u32>(scene->BGM_ID) != id ||
        scene->requestBgmWave_1 != wave1 || scene->requestBgmWave_2 != wave2) {
        scene->sceneChange(JAISoundID(id), scene->requestSeWave_1,
            scene->requestSeWave_2, wave1, wave2, scene->requestDemoWave, false);
    }
    sceneStartPending.store(true, std::memory_order_relaxed);
}

HookAction change_bgm_status_pre(ModContext*, void* args, void*, void*) {
    auto* sequence = mods::arg<Z2SeqMgr*>(args, 0);
    auto& status = mods::arg_ref<s32>(args, 1);
    const bool globalPalace = runtime_settings().style == Style::Normal ||
                              runtime_settings().style == Style::BlackAndWhite;
    const char* stage = dComIfGp_getStartStageName();
    const bool gameplayStageKnown = stage != nullptr && *stage != '\0';
    if (sequence != nullptr && globalPalace && active() && gameplayStageKnown &&
        !title_or_file_select_active()) {
        static s32 lastLoggedStatus = -1;
        if (status != 0 && status != lastLoggedStatus) {
            lastLoggedStatus = status;
            char message[160]{};
            std::snprintf(message, sizeof(message),
                "PalaceStatusOverride requested=%d stage=%s main=%08x",
                static_cast<int>(status), stage,
                static_cast<unsigned>(sequence->getMainBgmID()));
            svc_log->info(mod_ctx, message);
        }
        // The Palace hand uses statuses 100+ to select its combat layers.
        // Global Palace music must remain on the exterior arrangement.
        // If a save restored the exterior status byte while leaving the child
        // tracks in the hand mix, vanilla's same-status early return would
        // skip the volume reset. Force one real status transition so vanilla
        // reapplies all Palace child-track volumes.
        if (status != 0 && sequence->mBgmStatus == 0) {
            sequence->mBgmStatus = 0xff;
        }
        status = 0;
    }
    return HOOK_CONTINUE;
}
}
bool is_boss_bgm(u32 id) {
    switch (id) {
    case Z2BGM_FACE_OFF_BATTLE:
    case Z2BGM_BOOMERAMG_MONKEY:
    case Z2BGM_BOSSBABA_0:
    case Z2BGM_BOSSBABA_1:
    case Z2BGM_BOSSBABA_2:
    case Z2BGM_BOSSFIREMAN_0:
    case Z2BGM_BOSSFIREMAN_1:
    case Z2BGM_MAGNE_GORON:
    case Z2BGM_DEKUTOAD:
    case Z2BGM_BOSS_OCTAEEL_0:
    case Z2BGM_BOSS_OCTAEEL_1:
    case Z2BGM_VARIANT:
    case Z2BGM_BOSS_SNOWWOMAN_0:
    case Z2BGM_BOSS_SNOWWOMAN_1:
    case Z2BGM_IB_MBOSS:
    case Z2BGM_BOSS_ZANT:
    case Z2BGM_TN_MBOSS:
    case Z2BGM_GG_MBOSS:
    case Z2BGM_P_ZANT:
    case Z2BGM_VS_GANON_01:
    case Z2BGM_VS_GANON_02:
    case Z2BGM_VS_GANON_04:
    case Z2BGM_HARAGIGANT_BTL01:
    case Z2BGM_HARAGIGANT_BTL02:
    case Z2BGM_DRAGON_BTL01:
    case Z2BGM_DRAGON_BTL02:
    case Z2BGM_GOMA_BTL01:
    case Z2BGM_GOMA_BTL02:
    case Z2BGM_FACE_OFF_BATTLE2:
    case Z2BGM_FACE_OFF_BATTLE3:
    case Z2BGM_TN_MBOSS_LV9:
        return true;
    default:
        return false;
    }
}
ModResult initialize() {
    const auto directory = platform::custom_music_directory();
#if defined(__ANDROID__)
    if (!directory.empty()) {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            const std::string warning = "Android custom music folder could not be created: " +
                directory.string() + " (" + error.message() + ")";
            svc_log->warn(mod_ctx, warning.c_str());
        } else {
            const std::string message = "Android custom music folder: " + directory.string();
            svc_log->info(mod_ctx, message.c_str());
        }
    } else {
        svc_log->warn(mod_ctx,
            "Android custom music folder is unavailable; continuing without external MP3s.");
    }
#else
    if (directory.empty()) return MOD_UNAVAILABLE;
#endif
#if defined(__APPLE__)
    if (const auto userDirectory = mac_user_music_directory(); !userDirectory.empty()) {
        std::error_code error;
        std::filesystem::create_directories(userDirectory, error);
        const std::string message = "macOS custom music folder (preferred when a track exists): " +
            userDirectory.string();
        if (error) {
            const std::string warning = message + " (could not create: " + error.message() + ")";
            svc_log->warn(mod_ctx, warning.c_str());
        } else {
            svc_log->info(mod_ctx, message.c_str());
        }
    }
#endif
    if (!directory.empty()) {
        AstralMp3Ambient.open(music_file(directory, "Astral Plane.mp3"));
        AstralMp3Combat.open(music_file(directory, "Astral Plane CM.mp3"));
        DarkHourAmbient.open(music_file(directory, "tartarus 0d06.mp3"));
        DarkHourCombat.open(music_file(directory, "Mass Destruction.mp3"));
        MasterOfShadow.open(music_file(directory, "Master of Shadow.mp3"));
    }
    if (mods::hook::add_pre<RegisterMixCallback>(register_mix_pre) != MOD_OK)
        return MOD_UNSUPPORTED;
    mixHookInstalled = true;
    if (mods::hook::add_pre<SceneChange>(scene_change_pre) == MOD_OK)
        sceneHookInstalled = true;
    if (mods::hook::add_post<SetSceneName>(scene_name_post) == MOD_OK)
        sceneNameHookInstalled = true;
    if (!sceneHookInstalled && !sceneNameHookInstalled)
        return MOD_UNSUPPORTED;
    if (mods::hook::add_post<ProcessBgmFramework>(framework_post) == MOD_OK)
        frameworkHookInstalled = true;
    if (mods::hook::add_pre<ChangeBgmStatus>(change_bgm_status_pre) == MOD_OK) {
        palaceStatusHookInstalled = true;
        svc_log->info(mod_ctx, "Palace status override hook active");
    } else {
        svc_log->warn(mod_ctx, "Palace status override hook unavailable");
    }
    // Bind the same exported state read by DuskAudioSystem::RenderAudioSubframe.
    // Calling registerMixCallback alone is insufficient when another subsystem
    // has already cached/replaced the external callback during initialization.
    bind_vanilla_mixer();
    registered = true;
    svc_log->info(mod_ctx, "Custom MP3 mixer bound to vanilla Dusklight audio output");
    return MOD_OK;
}
void set_volume(float value) { TwilightMusicVolume.store(std::clamp(value, 0.0f, 1.0f)); }
void prepare_scene() { sceneStartPending.store(true); }
void suspend() {
    AstralMp3Ambient.setGain(0.0f, 0.0f, false, false, true);
    AstralMp3Combat.setGain(0.0f, 0.0f, false, false, true);
    DarkHourAmbient.setGain(0.0f, 0.0f, false, false, true);
    DarkHourCombat.setGain(0.0f, 0.0f, false, false, true);
    MasterOfShadow.setGain(0.0f, 0.0f, false, false, true);
    PalaceGain.store(1.0f);
    BattleGain.store(1.0f);
    BossGain.store(1.0f);
    BossNativeMain.store(0xffffffff);
    BossNativeSub.store(0xffffffff);
    NativeBgmMute.store(false);
    customOwnership.store(false, std::memory_order_relaxed);
    explicitlySuspended.store(true, std::memory_order_relaxed);
    sceneStartPending.store(true);
    sceneSelectionKnown = false;
}
void sequence(bool scene, bool eligible, int mode, float gain, bool scope, bool battle,
              float battleVolume, bool boss, float bossVolume, u32 bossMain, u32 bossSub) {
    update_sequence(scene, eligible, mode, gain, scope, battle, battleVolume, boss, bossVolume,
                    bossMain, bossSub);
}
void update() {
    // processBgmFramework is authoritative when its hook is available. Avoid
    // advancing fades twice if a host also invokes mod_update each frame.
    if (frameworkHookInstalled) return;
    update_from_manager(Z2GetSeqMgr());
}
void update_from_manager(Z2SeqMgr* p) {
    bind_vanilla_mixer();
    auto* status = Z2GetStatusMgr();
    if (!p) return;
    const u8 demo = status ? status->getDemoStatus() : 0;
    const u32 main = p->getMainBgmID();
    const u32 sub = p->getSubBgmID();
    const bool ordinary = sub == Z2BGM_BATTLE_NORMAL || sub == Z2BGM_BATTLE_TWILIGHT;
    const bool rawEnabled = active() && music_override_allowed();
    refresh_scene_music_selection(rawEnabled, p, status);
    const bool globalPalace = runtime_settings().style == Style::Normal ||
                              runtime_settings().style == Style::BlackAndWhite;
    if (rawEnabled && globalPalace && p->getMainBgmID() == Z2BGM_DUNGEON_LV8)
        p->changeBgmStatus(0);
    const bool customStyle = runtime_settings().style == Style::AstralPlane ||
                             runtime_settings().style == Style::DarkHour;
    const bool playerExists = dComIfGp_getPlayer(0) != nullptr;
    if (rawEnabled && customStyle) {
        customOwnership.store(true, std::memory_order_relaxed);
        explicitlySuspended.store(false, std::memory_order_relaxed);
    } else if (explicitlySuspended.load(std::memory_order_relaxed) || !active() ||
               !customStyle || playerExists) {
        // A live player plus a rejected policy is a genuine excluded scene,
        // not a transient room teardown.
        customOwnership.store(false, std::memory_order_relaxed);
    }
    const bool enabled = rawEnabled || customOwnership.load(std::memory_order_relaxed);
    const bool resetting = mDoRst::getResetData() != nullptr && mDoRst::isReset();
    const bool transitionCarry = customOwnership.load(std::memory_order_relaxed) && !playerExists;
    const bool protectedMusic = is_boss_bgm(main) || is_boss_bgm(sub) ||
                                fanfare(main) || fanfare(sub);
    // Some overworld scenes reserve/start a native stream when the clock enters
    // night. Dark Hour owns ambient music in ordinary gameplay, so that native
    // nighttime stream must not evict it. Cutscenes, resets, fanfares, and boss
    // music retain the normal safety gate.
    const bool darkHourNight = runtime_settings().style == Style::DarkHour &&
                               dKy_daynight_check() && demo == 0 && !protectedMusic;
    const bool streamSafe = p->getStreamBgmID() == 0xffffffffu || darkHourNight;
    const bool safe = transitionCarry || ((demo == 0 || demo == 1) &&
                      streamSafe && !resetting);
    const bool scope = enabled && safe &&
        (ordinary || !p->mFlags.mBattleBgmOff) &&
        (sub == 0xffffffffu || ordinary);
    const int mode = runtime_settings().style == Style::AstralPlane ? 1 :
                     runtime_settings().style == Style::DarkHour ? 2 : 0;
    // Pause and fanfare mute are live interruption controls. Keep them outside
    // the room-transition latch so item-get jingles and other fanfares can duck
    // the streamed replacement exactly as they duck vanilla BGM.
    const float interruptionGain = p->mBgmPause.get() * p->mFanfareMute.get();
    const float base = p->mAllBgmMaster.get() * p->mWindStone.get() * p->mTwilightGateVol;
    const float nativeSceneGain = base * (ordinary && scope ? 1.0f : p->mMainBgmMaster.get()) *
        p->mSceneBgm.get() * p->mStreamBgmMaster.get() * p->field_0x84.get() *
        p->field_0xa4.get();
    // Room/load-zone setup briefly forces one or more native scene faders to
    // zero even though gameplay and the selected replacement remain valid.
    // MFB's custom stream is independent of that transient native sequence
    // teardown, so preserve its last live gain and decoder position here.
    if (enabled && safe && nativeSceneGain > 0.001f)
        ambientGainLatch = std::clamp(nativeSceneGain, 0.0f, 1.0f);
    const float gain = enabled && safe ? ambientGainLatch * interruptionGain : 0.0f;
    const bool boss = enabled && (is_boss_bgm(main) || is_boss_bgm(sub));
    update_sequence(enabled, enabled && safe, enabled ? mode : 0, gain, scope,
        ordinary, base * interruptionGain * p->mStreamBgmMaster.get(), boss,
        base * interruptionGain, main, sub);
    apply_native_gains(p);

    // Temporary high-signal diagnostics: one line every five seconds identifies
    // whether silence originates before decoding, in the policy gate, or after
    // the vanilla audio thread consumes the callback.
    static u32 diagnosticFrames = 0;
    if (++diagnosticFrames >= 300) {
        diagnosticFrames = 0;
        char message[512]{};
        std::snprintf(message, sizeof(message),
            "MusicTrace calls=%llu peak=%.4f enabled=%d safe=%d mode=%d demo=%u "
            "main=%08x sub=%08x stream=%08x gain=%.4f fade=%.3f ambient=%.3f combat=%.3f",
            static_cast<unsigned long long>(customMixCalls.load(std::memory_order_relaxed)),
            customMixPeak.load(std::memory_order_relaxed), enabled ? 1 : 0, safe ? 1 : 0,
            mode, static_cast<unsigned>(demo), main, sub, p->getStreamBgmID(), gain,
            fade.astral(), DarkHourAmbient.audibleGain, DarkHourCombat.audibleGain);
        svc_log->info(mod_ctx, message);
    }
}
void shutdown() {
    if (frameworkHookInstalled) { mods::hook::uninstall<ProcessBgmFramework>(); frameworkHookInstalled = false; }
    if (palaceStatusHookInstalled) { mods::hook::uninstall<ChangeBgmStatus>(); palaceStatusHookInstalled = false; }
    if (sceneHookInstalled) { mods::hook::uninstall<SceneChange>(); sceneHookInstalled = false; }
    if (sceneNameHookInstalled) { mods::hook::uninstall<SetSceneName>(); sceneNameHookInstalled = false; }
    if (mixHookInstalled) {
        mods::hook::uninstall<RegisterMixCallback>();
        mixHookInstalled = false;
        JASDriver::extMixCallback = NativeMixCallback;
        JASDriver::sMixMode = NativeMixCallback ? MIX_MODE_INTERLEAVE : MIX_MODE_MONO;
    }
    registered = false;
    customMixCalls.store(0, std::memory_order_relaxed);
    customMixPeak.store(0.0f, std::memory_order_relaxed);
    ambientGainLatch = 1.0f;
    AstralMp3Ambient.close();
    AstralMp3Combat.close();
    DarkHourAmbient.close();
    DarkHourCombat.close();
    MasterOfShadow.close();
    fade = {};
    encounterFade = {};
    bossFade = {};
    lastTick = {};
    sceneStartPending.store(true);
    PalaceGain.store(1);
    BattleGain.store(1);
    BossGain.store(1);
    BossNativeMain.store(0xffffffff);
    BossNativeSub.store(0xffffffff);
    NativeBgmMute.store(false);
    customOwnership.store(false, std::memory_order_relaxed);
    explicitlySuspended.store(false, std::memory_order_relaxed);
    LiveSceneManager = nullptr;
    sceneSelectionKnown = false;
    previousReplacementSelection = false;
}
}
