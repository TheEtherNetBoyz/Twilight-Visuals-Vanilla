#include "../src/run_hold.hpp"
#include <cassert>
using twilight_visuals::running::RunHold;
int main() {
    using twilight_visuals::running::nativeRollChainPress;
    assert(nativeRollChainPress(true, true, false));
    assert(nativeRollChainPress(true, false, true));
    assert(!nativeRollChainPress(false, true, false)); // Holding never auto-rolls.
    assert(!nativeRollChainPress(true, false, false)); // Ordinary input still charges.
    RunHold input;
    assert(!input.update(true, true, true, 5));
    assert(input.update(false, false, true, 5)); // Early release rolls.
    assert(!input.update(false, false, true, 5));
    input.update(true, true, true, 5);
    for (int i = 0; i < 4; ++i) {
        input.update(false, true, true, 5);
        assert(!input.running());
    }
    input.update(false, true, true, 5);
    assert(input.running());
    assert(!input.update(false, false, true, 5)); // Running release does not roll.
    input.update(true, true, true, 5);
    input.update(false, true, false, 5); // Interrupted charge is discarded.
    assert(!input.update(false, false, true, 5));
    input.update(false, true, true, 5); // Holding without a new trigger cannot charge.
    assert(input.state == RunHold::State::Idle);
}
