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
    std::int32_t vertical_velocity = 0;
    std::int32_t curve = 0;
    std::uint8_t state = 1;
};

// Applies one state-1 PES ball update's angle and horizontal displacement.
// `tick` is the integer update context produced by PES; `tick.delta` is 10
// in the captured match state. This deliberately leaves vertical collision,
// speed loss and ownership transitions to separately verified code.
struct BallFlightTick {
    std::int32_t delta = 10;
};

// Convert the byte-sized attribute read by PES's conditional kick-power path
// into its additive speed adjustment. The caller must apply this only when
// the original player-state guard is true.
std::int32_t kick_power_adjustment(std::uint8_t attribute);

// Mirrors the launch-field writes in FUN_00e20190/FUN_00e1fdb0. The optional
// adjustment is the already-resolved result of the guarded player path.
void launch_ball(BallFlightState& ball, std::uint16_t heading,
                 std::int32_t requested_power, std::int32_t vertical_parameter,
                 std::int32_t guarded_power_adjustment = 0);

void step_ball_flight_horizontal(BallFlightState& ball, BallFlightTick tick);

// Applies the verified state-2 heading, curve, and position update. For the
// captured rolling range above 0x800, this includes PES's Y bob derived from
// the updated X/Z, heading, and intermediate movement speed. PES's final
// damped speed calculation still depends on an unresolved input, so the
// caller must supply the speed observed/recovered for this tick.
void step_ball_ground_motion(BallFlightState& ball, BallFlightTick tick,
                             std::int32_t final_speed);

// Applies only the pre-contact vertical position equation from
// FUN_00e1e3e0's state-1 branch. The temporary-state velocity is measured in
// 1/2560 world-unit increments per tick. Landing, bounce, and vertical-speed
// changes are separate branches and remain unimplemented.
void step_ball_flight_vertical_position(BallFlightState& ball, BallFlightTick tick);

}  // namespace gameplay
