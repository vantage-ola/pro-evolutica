#pragma once

#include <cstdint>

namespace gameplay {

// PES state-1 horizontal flight fields in the original game's units.
// The complete vertical/contact response and speed update are not part of
// this recovered step yet.
struct BallFlightState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::uint16_t angle = 0;
    std::uint16_t spin = 0;
    std::int32_t speed = 0;
    std::uint8_t direction_flags = 0;
};

// Applies one state-1 PES ball update's angle and horizontal displacement.
// `tick` is the integer update context produced by PES; `tick.delta` is 10
// in the captured match state. This deliberately leaves vertical collision,
// speed loss and ownership transitions to separately verified code.
struct BallFlightTick {
    std::int32_t delta = 10;
};

void step_ball_flight_horizontal(BallFlightState& ball, BallFlightTick tick);

}  // namespace gameplay
