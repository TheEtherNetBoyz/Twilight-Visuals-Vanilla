#pragma once

namespace twilight_visuals::running {
constexpr bool nativeRollChainPress(bool trigger, bool rolling, bool justFinishedRoll) {
    return trigger && (rolling || justFinishedRoll);
}
struct RunHold {
    enum class State { Idle, Ready, Running };
    State state = State::Idle;
    float remaining = 0;
    // Wolf charge order: release first, then advance preparation.
    bool update(bool trigger, bool down, bool eligible, float readyFrames) {
        if (!eligible) { state = State::Idle; return false; }
        if (state == State::Idle) {
            if (trigger && down) {
                state = State::Ready;
                remaining = readyFrames;
            }
        } else if (!down) {
            const bool roll = state == State::Ready;
            state = State::Idle;
            return roll;
        } else if (state == State::Ready) {
            remaining -= 1.0f;
            if (remaining <= 0.0f) state = State::Running;
        }
        return false;
    }
    bool running() const { return state == State::Running; }
};
}
