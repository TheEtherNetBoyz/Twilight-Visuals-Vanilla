#include "platform.hpp"

#include "service_refs.hpp"

#include <cstdlib>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace twilight_visuals::platform {

std::filesystem::path executable_directory() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 1024;
    for (;;) {
        std::vector<char> buffer(size);
        uint32_t actual_size = size;
        if (_NSGetExecutablePath(buffer.data(), &actual_size) == 0)
            return std::filesystem::path(buffer.data()).parent_path();
        if (actual_size <= size) return {};
        size = actual_size;
    }
#elif defined(__linux__)
    std::vector<char> buffer(1024);
    for (;;) {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) return {};
        if (static_cast<size_t>(length) < buffer.size()) {
            buffer.resize(static_cast<size_t>(length));
            return std::filesystem::path(buffer.data()).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return {};
#endif
}

std::filesystem::path custom_asset_directory() {
#if defined(__ANDROID__)
    if (svc_host == nullptr || svc_host->data_dir == nullptr) return {};
    const char* dataDirectory = nullptr;
    if (svc_host->data_dir(mod_ctx, &dataDirectory) != MOD_OK ||
        dataDirectory == nullptr || *dataDirectory == '\0') return {};
    return std::filesystem::path(dataDirectory) / "custom assets";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return {};
    return std::filesystem::path(home) /
        "Library/Application Support/TwilitRealm/Dusklight/Twilight Visuals/custom assets";
#else
    const auto directory = executable_directory();
    return directory.empty() ? std::filesystem::path{} : directory / "custom assets";
#endif
}

std::filesystem::path custom_music_directory() {
    const auto directory = custom_asset_directory();
    return directory.empty() ? std::filesystem::path{} : directory / "music";
}

std::filesystem::path custom_animation_directory() {
    const auto directory = custom_asset_directory();
    return directory.empty() ? std::filesystem::path{} : directory / "animations";
}

}  // namespace twilight_visuals::platform
