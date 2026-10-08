#pragma once

#include <filesystem>

namespace twilight_visuals::platform {

// Return the directory containing the current game executable.
// An empty path means the platform could not provide it.
std::filesystem::path executable_directory();

// Return the writable directory used for external mod data. Desktop builds
// retain the executable-directory behavior used by the original mod, while
// Android maps this to Dusklight's persistent per-mod data directory.
std::filesystem::path custom_music_directory();

// Return the preferred executable-adjacent directory for external test
// animations. Desktop builds place this beside the game executable; Android
// uses the persistent per-mod data directory.
std::filesystem::path custom_animation_directory();

}  // namespace twilight_visuals::platform
