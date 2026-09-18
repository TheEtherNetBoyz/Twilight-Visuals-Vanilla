#include "compat.hpp"
#include "sky.hpp"

namespace twilight_visuals::compat {

float get_master_volume() { return 1.0f; }

bool get_authored_sky(VisualSkybox& outSky, u8 variant) {
    struct SkyVariantSource { const char* stageName; u8 layer; u8 paletteSlot; };
    static constexpr SkyVariantSource kSources[] = {
        {"F_SP108", 14, 0}, {"F_SP108", 0, 5}, {"F_SP121", 0, 1},
        {"F_SP121", 0, 4}, {"F_SP114", 0, 0}, {"F_SP108", 14, 0},
        {"F_SP109", 14, 0}, {"F_SP115", 14, 0}, {"D_MN08", 0, 0},
        {"F_SP117", 0, 0}, {"F_SP114", 0, 0}, {"F_SP124", 0, 0},
        {"F_SP115", 0, 0}, {"F_SP127", 0, 0}, {"F_SP103", 0, 0},
        {"F_SP121", 0, 0}, {"F_SP116", 0, 0},
    };
    if (variant >= sizeof(kSources) / sizeof(kSources[0])) return false;
    const auto& source = kSources[variant];
    return sky::read(&outSky, source.stageName, source.layer, source.paletteSlot,
                     source.layer == 14 ? 10 : 0);
}

}  // namespace twilight_visuals::compat
