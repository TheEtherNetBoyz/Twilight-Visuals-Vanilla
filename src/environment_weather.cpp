#include "environment_weather.hpp"

#include "runtime.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo_wether.h"
#include "m_Do/m_Do_audio.h"
#include "SSystem/SComponent/c_math.h"

namespace twilight_visuals::environment::weather {
namespace {

struct WeatherState {
    bool saved{};
    int rainCount{};
    int baseRainCount{};
    int snowCount{};
    u8 weather{};
    u8 weatherPat0{};
    u8 weatherPat1{};
    u8 prevGather{0xFF};
    u8 currGather{0xFF};
    float gatherRatio{-1.0f};
    u8 patMode{};
    u8 patModeGather{};
    float patternRatio{1.0f};
    float fogNear{};
    float fogFar{};
    float fogOverrideNear{};
    float fogOverrideFar{};
    float fogOverrideRatio{};
    u8 moyaMode{};
    int moyaCount{};
    u8 snowFogMode{};
    int thunderMode{};
    u8 thunderStatus{};
    cXyz* windOverride{};
    float customWindPower{};
    u8 teachWindExistence{};
};

WeatherState g_state;
cXyz g_stormWind(1.0f, 0.0f, 0.0f);
cXyz g_windStormWind(1.75f, 0.0f, 0.0f);
int g_windGustTimer{};
bool g_windGustActive{};

}  // namespace

void restore() {
    WeatherState& saved = g_state;
    g_windGustTimer = 0;
    g_windGustActive = false;
    if (!saved.saved) return;
    dKyw_rain_set(saved.rainCount);
    g_env_light.base_raincnt = saved.baseRainCount;
    g_env_light.mSnowCount = saved.snowCount;
    g_env_light.mColpatWeather = saved.weather;
    g_env_light.wether_pat0 = saved.weatherPat0;
    g_env_light.wether_pat1 = saved.weatherPat1;
    g_env_light.mColpatPrevGather = saved.prevGather;
    g_env_light.mColpatCurrGather = saved.currGather;
    g_env_light.mColPatBlendGather = saved.gatherRatio;
    g_env_light.mColPatMode = saved.patMode;
    g_env_light.mColPatModeGather = saved.patModeGather;
    g_env_light.pat_ratio = saved.patternRatio;
    g_env_light.mFogNear = saved.fogNear;
    g_env_light.mFogFar = saved.fogFar;
    g_env_light.field_0x11ec = saved.fogOverrideNear;
    g_env_light.field_0x11f0 = saved.fogOverrideFar;
    g_env_light.field_0x11f4 = saved.fogOverrideRatio;
    g_env_light.mMoyaMode = saved.moyaMode;
    g_env_light.mMoyaCount = saved.moyaCount;
    g_env_light.field_0xe92 = saved.snowFogMode;
    g_env_light.mThunderEff.mMode = saved.thunderMode;
    g_env_light.mThunderEff.mStatus = saved.thunderStatus;
    g_env_light.global_wind_influence.vec_override = saved.windOverride;
    g_env_light.custom_windpower = saved.customWindPower;
    g_env_light.TeachWind_existence = saved.teachWindExistence;
    saved.saved = false;
}

void apply() {
    const Weather selectedWeather = runtime_settings().weather;
    if (selectedWeather == Weather::Current) {
        restore();
        return;
    }

    WeatherState& saved = g_state;
    if (!saved.saved) {
        saved.saved = true;
        saved.rainCount = g_env_light.raincnt;
        saved.baseRainCount = g_env_light.base_raincnt;
        saved.snowCount = g_env_light.mSnowCount;
        saved.weather = g_env_light.mColpatWeather;
        saved.weatherPat0 = g_env_light.wether_pat0;
        saved.weatherPat1 = g_env_light.wether_pat1;
        saved.prevGather = g_env_light.mColpatPrevGather;
        saved.currGather = g_env_light.mColpatCurrGather;
        saved.gatherRatio = g_env_light.mColPatBlendGather;
        saved.patMode = g_env_light.mColPatMode;
        saved.patModeGather = g_env_light.mColPatModeGather;
        saved.patternRatio = g_env_light.pat_ratio;
        saved.fogNear = g_env_light.mFogNear;
        saved.fogFar = g_env_light.mFogFar;
        saved.fogOverrideNear = g_env_light.field_0x11ec;
        saved.fogOverrideFar = g_env_light.field_0x11f0;
        saved.fogOverrideRatio = g_env_light.field_0x11f4;
        saved.moyaMode = g_env_light.mMoyaMode;
        saved.moyaCount = g_env_light.mMoyaCount;
        saved.snowFogMode = g_env_light.field_0xe92;
        saved.thunderMode = g_env_light.mThunderEff.mMode;
        saved.thunderStatus = g_env_light.mThunderEff.mStatus;
        saved.windOverride = g_env_light.global_wind_influence.vec_override;
        saved.customWindPower = g_env_light.custom_windpower;
        saved.teachWindExistence = g_env_light.TeachWind_existence;
    }

    const bool windStorm = selectedWeather == Weather::WindStorm;
    const bool snowStorm = selectedWeather == Weather::SnowStorm;
    const bool storm = windStorm || snowStorm;
    if (storm) {
        if (g_windGustTimer <= 0) {
            g_windGustActive = !g_windGustActive;
            g_windGustTimer = g_windGustActive ? 60 + static_cast<int>(cM_rndF(361.0f)) :
                                                60 + static_cast<int>(cM_rndF(120.0f));
        }
        --g_windGustTimer;
    } else {
        g_windGustTimer = 0;
        g_windGustActive = false;
    }

    const bool denseFog = snowStorm || selectedWeather == Weather::HeavyFog;
    if (snowStorm) {
        g_env_light.mMoyaMode = 0;
        g_env_light.mMoyaCount = 50;
        g_env_light.field_0xe92 = 1;
        g_env_light.field_0x11ec = 200.0f;
        g_env_light.field_0x11f0 = 2200.0f;
        g_env_light.field_0x11f4 = 1.0f;
        g_mEnvSeMgr.setSnowPower(127.0f);
    } else if (selectedWeather == Weather::HeavyFog) {
        g_env_light.mMoyaMode = 2;
        g_env_light.mMoyaCount = 50;
        g_env_light.field_0xe92 = 0;
        g_env_light.field_0x11ec = 200.0f;
        g_env_light.field_0x11f0 = 2200.0f;
        g_env_light.field_0x11f4 = 1.0f;
    } else {
        g_env_light.mMoyaMode = saved.moyaMode;
        g_env_light.mMoyaCount = saved.moyaCount;
        g_env_light.field_0xe92 = saved.snowFogMode;
        g_env_light.field_0x11ec = saved.fogOverrideNear;
        g_env_light.field_0x11f0 = saved.fogOverrideFar;
        g_env_light.field_0x11f4 = saved.fogOverrideRatio;
    }

    const bool bloodRain = selectedWeather == Weather::BloodRain;
    const bool wet = selectedWeather == Weather::Rain || selectedWeather == Weather::Lightning ||
                     windStorm;
    const bool raining = wet || bloodRain;
    const u8 pattern = wet ? 1 : (selectedWeather == Weather::Snow || snowStorm) ? 2 : 0;
    g_env_light.mColpatWeather = pattern;
    g_env_light.wether_pat0 = pattern;
    g_env_light.wether_pat1 = pattern;
    g_env_light.mColpatPrevGather = 0xFF;
    g_env_light.mColpatCurrGather = 0xFF;
    g_env_light.mColPatBlendGather = -1.0f;
    g_env_light.mColPatMode = 0;
    g_env_light.mColPatModeGather = 0;
    g_env_light.pat_ratio = 1.0f;

    if (raining) {
        dKyw_rain_set(250);
        g_env_light.mSnowCount = 0;
    } else if (selectedWeather == Weather::Snow || snowStorm) {
        dKyw_rain_set(0);
        g_env_light.mSnowCount = 500;
    } else {
        dKyw_rain_set(0);
        g_env_light.mSnowCount = 0;
    }

    const int thunderMode = selectedWeather == Weather::Lightning ? 1 : 0;
    if (thunderMode == 0 && g_env_light.mThunderEff.mMode != 0)
        g_env_light.mThunderEff.mStatus = 0;
    g_env_light.mThunderEff.mMode = thunderMode;

    if (storm) {
        g_env_light.global_wind_influence.vec_override =
            windStorm ? &g_windStormWind : &g_stormWind;
        g_env_light.custom_windpower = g_windGustActive ? 1.0f : 0.0f;
        g_env_light.TeachWind_existence = 1;
    } else {
        g_env_light.global_wind_influence.vec_override = saved.windOverride;
        g_env_light.custom_windpower = saved.customWindPower;
        g_env_light.TeachWind_existence = saved.teachWindExistence;
    }

    if (denseFog) {
        g_env_light.mFogNear = 200.0f;
        g_env_light.mFogFar = 2200.0f;
    } else {
        g_env_light.mFogNear = saved.fogNear;
        g_env_light.mFogFar = saved.fogFar;
    }
}

void area_reloaded() {
    // Previous area's baseline must never be restored into the new area.
    g_state = {};
    g_windGustTimer = 0;
    g_windGustActive = false;
}

}  // namespace twilight_visuals::environment::weather
