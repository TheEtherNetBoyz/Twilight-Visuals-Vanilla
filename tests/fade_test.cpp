#include "../src/TwilightMusicFade.h"
#include <cassert>
#include <cmath>

int main() {
    dusk::audio::TwilightMusicFade fade;
    assert(fade.palace() == 1.0f && fade.astral() == 0.0f);
    float last = 0.0f;
    for (int i = 0; i < 150; ++i) {
        fade.update(true, 1.0f / 60.0f);
        assert(fade.position >= last && fade.position <= 1.0f);
        assert(fade.palace() * fade.astral() == 0.0f);
        last = fade.position;
    }
    assert(fade.astral() == 1.0f);
    for (int i = 0; i < 210; ++i) {
        fade.update(false, 1.0f / 60.0f, 3.0f);
        assert(fade.position <= last && fade.position >= 0.0f);
        assert(fade.palace() * fade.astral() == 0.0f);
        last = fade.position;
    }
    assert(fade.palace() == 1.0f);
    // Map startup must mute Palace even on a zero-elapsed first audio tick.
    fade.select(true, true, 0.0f, true);
    assert(fade.palace() == 0.0f && fade.astral() == 1.0f);
    fade.select(true, true, 0.0f);
    assert(fade.astral() == 1.0f);
    // In-game setting changes still take the normal fade path.
    fade.select(false, true, 1.0f / 60.0f);
    assert(fade.position > 0.0f && fade.position < 1.0f);
    fade.select(true, false, 0);
    assert(fade.astral() == 1.0f);
    fade.select(false, false, 0);
    assert(fade.palace() == 1.0f);
}
