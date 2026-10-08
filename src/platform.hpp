#pragma once

#include <filesystem>

namespace twilight_visuals::platform {

// Return the directory containing the current game executable.
// An empty path means the platform could not provide it.
std::filesystem::path executable_directory();

// Return the shared root for all user-provided Twilight Visuals assets.
// Windows uses a folder beside Dusklight.exe, macOS uses Application Support,
// and Android uses Dusklight's persistent per-mod data directory.
std::filesystem::path custom_asset_directory();

// Return the music subdirectory within the shared custom asset root.
std::filesystem::path custom_music_directory();

// Return the animation subdirectory within the shared custom asset root.
std::filesystem::path custom_animation_directory();

}  // namespace twilight_visuals::platform
