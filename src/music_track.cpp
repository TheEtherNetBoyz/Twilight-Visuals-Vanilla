#define DR_MP3_IMPLEMENTATION
#include "music_track.hpp"

#include "external_assets.hpp"
#include "service_refs.hpp"
#include "Z2AudioLib/Z2Param.h"

#include <algorithm>
#include <string>

namespace twilight_visuals::music {
namespace {

bool init_mp3(drmp3* decoder, const std::filesystem::path& path) {
#if defined(_WIN32)
    return drmp3_init_file_w(decoder, path.c_str(), nullptr) != 0;
#else
    const auto utf8Path = path.u8string();
    return drmp3_init_file(decoder,
        reinterpret_cast<const char*>(utf8Path.c_str()), nullptr) != 0;
#endif
}

}  // namespace

void Track::close() {
    if (available) drmp3_uninit(&decoder);
    decoder = {};
    available = false;
    playbackStarted = false;
    primed = false;
    audibleGain = 0.0f;
    frameIndex = 0;
    frameCount = 0;
    position = 0.0;
}

void Track::open(const std::filesystem::path& file) {
    close();
    path = file;
    const auto fileStatus = external_assets::inspect_file(path);
    if (!fileStatus) {
        const std::string message = "Music unavailable (" + fileStatus.reason + "): " +
            external_assets::display_path(path);
        svc_log->warn(mod_ctx, message.c_str());
        return;
    }

    available = init_mp3(&decoder, path);
    if (available && (decoder.channels == 0 || decoder.channels > 2 || decoder.sampleRate == 0))
        close();
    const std::string message =
        std::string(available ? "Streaming music: " : "Music unavailable (unsupported MP3): ") +
        external_assets::display_path(path);
    svc_log->write(mod_ctx, available ? LOG_LEVEL_INFO : LOG_LEVEL_WARN, message.c_str());
}

bool Track::rewind() {
    if (!available) return false;
    if (!drmp3_seek_to_pcm_frame(&decoder, 0)) {
        drmp3_uninit(&decoder);
        decoder = {};
        available = init_mp3(&decoder, path);
    }
    frameIndex = 0;
    frameCount = 0;
    return available;
}

bool Track::next(std::array<float, 2>& sample) {
    if (frameIndex == frameCount) {
        frameIndex = 0;
        frameCount = static_cast<std::size_t>(drmp3_read_pcm_frames_f32(
            &decoder, decoded.size() / decoder.channels, decoded.data()));
        if (!frameCount) {
            if (!rewind()) return false;
            frameCount = static_cast<std::size_t>(drmp3_read_pcm_frames_f32(
                &decoder, decoded.size() / decoder.channels, decoded.data()));
            if (!frameCount) return false;
        }
    }
    const std::size_t index = frameIndex++ * decoder.channels;
    sample = {decoded[index], decoded[index + (decoder.channels == 2 ? 1 : 0)]};
    return true;
}

void Track::setGain(float target, float elapsed, bool smoothReduction,
                    bool rewindWhenSilent, bool immediate) {
    if (!available) return;
    if (target > 0.0f) playbackStarted = true;
    const float step = std::clamp(elapsed, 0.0f, 0.05f);
    audibleGain = immediate ? target : smoothReduction
        ? audibleGain + std::clamp(target - audibleGain, -step, step)
        : std::min(target, audibleGain + step);
    if (rewindWhenSilent && target <= 0.0f && audibleGain <= 0.0001f && playbackStarted) {
        rewind();
        playbackStarted = false;
        primed = false;
        position = 0.0;
    }
}

void Track::mix(float* output, std::uint32_t frames, std::uint32_t rate,
                float calibration, float volume, bool pauseWhenSilent) {
    if (!available || !playbackStarted || !rate ||
        (pauseWhenSilent && audibleGain <= 0.0001f)) return;
    if (!primed) {
        if (!next(a) || !next(b)) return;
        primed = true;
    }
    const double step = static_cast<double>(decoder.sampleRate) / rate;
    const float gain = audibleGain * calibration * volume * Z2Param::VOL_BGM_DEFAULT;
    for (std::uint32_t i = 0; i < frames; ++i) {
        for (std::uint32_t channel = 0; channel < 2; ++channel) {
            output[2 * i + channel] +=
                (a[channel] + (b[channel] - a[channel]) * static_cast<float>(position)) * gain;
        }
        position += step;
        while (position >= 1.0) {
            a = b;
            if (!next(b)) return;
            position -= 1.0;
        }
    }
}

}  // namespace twilight_visuals::music
