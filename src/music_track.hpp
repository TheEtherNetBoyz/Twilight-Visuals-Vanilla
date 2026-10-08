#pragma once

#include "third_party/dr_mp3.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace twilight_visuals::music {

// Owns one external MP3 decoder and its small resampling/mixing buffer.
// Scene selection and Dusklight hooks remain in music.cpp.
class Track {
public:
    Track() = default;
    Track(const Track&) = delete;
    Track& operator=(const Track&) = delete;

    void open(const std::filesystem::path& file);
    void close();
    void setGain(float target, float elapsed, bool smoothReduction = false,
                 bool rewindWhenSilent = false, bool immediate = false);
    void mix(float* output, std::uint32_t frames, std::uint32_t rate,
             float calibration, float volume, bool pauseWhenSilent = false);

    bool isAvailable() const { return available; }
    float currentGain() const { return audibleGain; }

private:
    bool rewind();
    bool next(std::array<float, 2>& sample);

    drmp3 decoder{};
    std::filesystem::path path;
    bool available = false;
    bool playbackStarted = false;
    float audibleGain = 0.0f;
    std::array<float, 4096> decoded{};
    std::size_t frameIndex = 0;
    std::size_t frameCount = 0;
    std::array<float, 2> a{};
    std::array<float, 2> b{};
    bool primed = false;
    double position = 0.0;
};

}  // namespace twilight_visuals::music
