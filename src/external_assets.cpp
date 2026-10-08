#include "external_assets.hpp"

#include <fstream>
#include <system_error>
#include <utility>

namespace twilight_visuals::external_assets {
namespace {

Status success() {
    return {true, {}};
}

Status failure(std::string reason) {
    return {false, std::move(reason)};
}

}  // namespace

std::string display_path(const std::filesystem::path& path) {
    const auto encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

Status ensure_directory(const std::filesystem::path& directory) {
    if (directory.empty()) return failure("directory path is unavailable");

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return failure("directory could not be created: " + error.message());

    const bool isDirectory = std::filesystem::is_directory(directory, error);
    if (error) return failure("directory could not be inspected: " + error.message());
    if (!isDirectory) return failure("path exists but is not a directory");
    return success();
}

Status inspect_file(const std::filesystem::path& path) {
    if (path.empty()) return failure("file path is unavailable");

    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) return failure("file could not be checked: " + error.message());
    if (!exists) return failure("file is missing");

    const bool regularFile = std::filesystem::is_regular_file(path, error);
    if (error) return failure("file could not be inspected: " + error.message());
    if (!regularFile) return failure("path is not a regular file");

    const auto size = std::filesystem::file_size(path, error);
    if (error) return failure("file size could not be read: " + error.message());
    if (size == 0) return failure("file is empty");
    return success();
}

Status read_binary(const std::filesystem::path& path,
                   std::vector<unsigned char>& output) {
    output.clear();
    const Status inspection = inspect_file(path);
    if (!inspection) return inspection;

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return failure("file could not be opened");

    const std::streamsize size = stream.tellg();
    if (size <= 0) return failure("file is empty or unreadable");
    stream.seekg(0, std::ios::beg);
    if (!stream) return failure("file could not be rewound before reading");

    output.resize(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(output.data()), size)) {
        output.clear();
        return failure("file could not be completely read");
    }
    return success();
}

}  // namespace twilight_visuals::external_assets
