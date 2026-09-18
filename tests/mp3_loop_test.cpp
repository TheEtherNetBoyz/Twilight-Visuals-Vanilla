#define DR_MP3_IMPLEMENTATION
#include "../src/third_party/dr_mp3.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cmath>

int wmain(int argc, wchar_t** argv) {
    assert(argc > 1);
    for (int file = 1; file < argc; ++file) {
        drmp3 decoder{};
        assert(drmp3_init_file_w(&decoder, argv[file], nullptr));
        assert(decoder.channels > 0 && decoder.channels <= 2);
        std::array<float, 512> first{}, again{};
        const auto count = drmp3_read_pcm_frames_f32(&decoder, 256, first.data());
        assert(count == 256);
        // MP3 metadata can overestimate the sample count. Exercise the same
        // decoder-EOF path as playback instead of assuming a precise duration.
        drmp3_uint64 frames = count;
        while (const auto decoded = drmp3_read_pcm_frames_f32(&decoder, 256, again.data()))
            frames += decoded;
        assert(frames > 256);
        assert(drmp3_read_pcm_frames_f32(&decoder, 256, again.data()) == 0);
        assert(drmp3_seek_to_pcm_frame(&decoder, 0));
        assert(drmp3_read_pcm_frames_f32(&decoder, 256, again.data()) == count);
        for (size_t i = 0; i < count * decoder.channels; ++i)
            assert(std::abs(first[i] - again[i]) < 0.00001f);
        drmp3_uninit(&decoder);
        std::wprintf(L"PASS decode, EOF, rewind: %ls\n", argv[file]);
    }
}
