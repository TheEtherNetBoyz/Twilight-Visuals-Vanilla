#include "environment_bloom.hpp"

#include "environment.hpp"
#include "runtime.hpp"
#include "boundary.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_data.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_graphic.h"
#include "SSystem/SComponent/c_math.h"

#include <algorithm>
#include <cstring>

namespace twilight_visuals::environment::bloom {
namespace {

bool environment_active() {
    const char* stage = dComIfGp_getStartStageName();
    return visual_effects_active() && stage != nullptr && !palace_excluded();
}

bool dark_hour_dungeon_indoor() {
    if (!dark_hour_indoor()) return false;
    const char* stage = dComIfGp_getStartStageName();
    return stage != nullptr &&
           (std::strncmp(stage, "D_MN", 4) == 0 ||
            std::strncmp(stage, "D_SB", 4) == 0);
}

}  // namespace

void apply_profile() {
    if (!environment_active()) return;
    const bool normalTwilight = runtime_settings().style == Style::Normal;
    if (normalTwilight && dComIfG_play_c::getLayerNo(0) == 14) return;
    if (normalTwilight && boundary::using_authored_twilight_environment()) return;
    // The vanilla global-Twilight option owns bloom unconditionally. Preserve
    // the older wolf/event exclusions only for the custom visual styles.
    if (!normalTwilight &&
        (daPy_py_c::checkNowWolfPowerUp() || g_env_light.field_0x12fc >= 0)) return;
    const dKydata_BloomInfo_c* profile = dKyd_BloomInf_tbl_getp(1);
    if (profile == nullptr) return;
    auto* bloom = mDoGph_gInf_c::getBloom();
    // Dusklight's vanilla global-Twilight option explicitly allocates bloom in
    // rooms whose native environment has it disabled. Merely copying profile 1
    // into an unallocated bloom object leaves Normal Twilight dark and flat.
    if (normalTwilight) bloom->create();
    // Dark Hour uses the same authored Twilight bloom preset. Only the color is
    // changed below to keep its green identity; threshold, blur, density, and
    // blend strength must remain identical to regular Twilight.
    if (normalTwilight) {
        bloom->setPoint(profile->info.mThreshold);
        bloom->setBlureSize(profile->info.mBlurAmount);
        bloom->setBlureRatio(profile->info.mDensity);
        bloom->setBlendColor({profile->info.mColorR, profile->info.mColorG,
                              profile->info.mColorB, profile->info.mOrigDensity});
        bloom->setMonoColor({profile->info.mSaturateSubtractR,
                             profile->info.mSaturateSubtractG,
                             profile->info.mSaturateSubtractB,
                             profile->info.mSaturateSubtractA});
        bloom->setEnable(1);
        bloom->setMode(profile->info.mType != BLOOM_CLEAR);
        return;
    }
    const bool indoor = dark_hour_indoor();
    // Indoors, preserve the blue base grade but let bright authored details
    // bloom in the exterior Dark Hour green. This creates localized glow
    // without recoloring the room's ambient light.
    const bool dungeonIndoor = dark_hour_dungeon_indoor();
    const bool reducedOutdoor = reduced_dark_hour_outdoor();
    const f32 desertBloomScale = gerudo_desert() ? 0.78f : 1.0f;
    const f32 darkHourScale = (reducedOutdoor ? reduced_dark_hour_outdoor_scale()
        : (dungeonIndoor ? 0.34f : (indoor ? 0.58f : 1.0f))) * desertBloomScale;
    const int threshold = reducedOutdoor
        ? std::min(255, static_cast<int>(profile->info.mThreshold) + 34)
        : indoor
        ? std::min(255, static_cast<int>(profile->info.mThreshold) +
                            (dungeonIndoor ? 56 : 32))
        : profile->info.mThreshold;
    bloom->setPoint(static_cast<u8>(threshold));
    static s16 pulsePhase{};
    const f32 pulse = cM_ssin(pulsePhase);
    pulsePhase += static_cast<s16>(cM_rndF(2000.0f) + 500.0f);
    const f32 blur = profile->info.mBlurAmount * darkHourScale * (1.0f + pulse * 0.2f);
    bloom->setBlureSize(static_cast<u8>(std::clamp(blur, 0.0f, 255.0f)));
    bloom->setBlureRatio(profile->info.mDensity * darkHourScale);
    bloom->setBlendColor({profile->info.mColorR, profile->info.mColorG,
                          profile->info.mColorB,
                          static_cast<u8>(profile->info.mOrigDensity * darkHourScale)});
    bloom->setMonoColor({profile->info.mSaturateSubtractR,
                         profile->info.mSaturateSubtractG,
                         profile->info.mSaturateSubtractB,
                         profile->info.mSaturateSubtractA});
    bloom->setEnable(profile->info.mThreshold < 0xFF);
    bloom->setMode(profile->info.mType != BLOOM_CLEAR);
}

}  // namespace twilight_visuals::environment::bloom
