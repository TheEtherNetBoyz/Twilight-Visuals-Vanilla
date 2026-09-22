#pragma once

#include <filesystem>

namespace twilight_visuals::platform {

// Return the directory containing the current game executable.
// An empty path means the platform could not provide it.
std::filesystem::path executable_directory();

}  // namespace twilight_visuals::platform
