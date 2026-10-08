#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace twilight_visuals::external_assets {

struct Status {
    bool ok = false;
    std::string reason;

    explicit operator bool() const { return ok; }
};

// Create a directory when needed and verify that the resulting path is a
// directory. Failures are returned to the calling feature for contextual logs.
Status ensure_directory(const std::filesystem::path& directory);

// Verify that a path is a non-empty regular file.
Status inspect_file(const std::filesystem::path& path);

// Read a complete non-empty file into memory.
Status read_binary(const std::filesystem::path& path,
                   std::vector<unsigned char>& output);

// Return a UTF-8 path suitable for logs on every supported desktop platform.
std::string display_path(const std::filesystem::path& path);

}  // namespace twilight_visuals::external_assets
