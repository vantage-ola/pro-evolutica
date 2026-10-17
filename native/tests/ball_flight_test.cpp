#include "../gameplay/ball_flight.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

void captured_state1_steps() {
    // Each incoming speed is the best-fit integer from the captured X/Z delta;
    // none of the three dumps stores the previous speed in the history ring.
    struct Case {
        gameplay::BallFlightState input;
        float expected_x;
        float expected_z;
        std::uint16_t expected_angle;
        float x_tolerance;
    };
    constexpr Case cases[] = {
        {{-8166.50390625f, -32.41796875f, 1476.345703125f, 16826, 191, 18292, 0},
         -8169.3984375f, 1404.962890625f, 16815, 0.04f},
        {{-9855.8359375f, -52.37109375f, -2294.54296875f, 38034, 156, 16352, 1},
         -9911.6796875f, -2263.58984375f, 38047, 0.001f},
        {{10619.6484375f, -348.109375f, 447.3046875f, 63266, 192, 25916, 0},
         10718.45703125f, 469.2265625f, 63258, 0.001f},
    };
    for (const auto& test : cases) {
        auto ball = test.input;
        gameplay::step_ball_flight_horizontal(ball, {10});
        check(ball.angle == test.expected_angle,
              "PES spin and position jitter changed the wrong heading");
        check(std::abs(ball.x - test.expected_x) < test.x_tolerance,
              "Recovered flight step missed a captured lateral position");
        check(std::abs(ball.z - test.expected_z) < 0.004f,
              "Recovered flight step missed a captured forward position");
        check(ball.y == test.input.y && ball.speed == test.input.speed,
              "Horizontal step must leave vertical/contact and speed state untouched");
    }
}

void original_kick_launch_fields() {
    gameplay::BallFlightState ground_ball;
    ground_ball.y = -28.0f;
    ground_ball.spin = 123;
    gameplay::launch_ball(ground_ball, 0x1234, 12001, 0);
    check(ground_ball.angle == 0x1234 && ground_ball.speed == 12001,
          "PES launch must copy heading and requested speed");
    check(ground_ball.curve == 3000 && ground_ball.spin == 0,
          "PES kick wrapper must derive curve from power and clear spin");
    check(ground_ball.vertical_velocity == 0 && ground_ball.state == 2,
          "A powered ground kick with no vertical impulse must enter rolling state");

    gameplay::BallFlightState lofted_ball;
    lofted_ball.y = -28.0f;
    gameplay::launch_ball(lofted_ball, 0xffff, 9000, 8);
    check(lofted_ball.vertical_velocity == -8 * 0xbc && lofted_ball.state == 1,
          "PES vertical launch parameter must scale by -0xbc and enter flight state");

    gameplay::BallFlightState airborne_ball;
    airborne_ball.y = -52.0f;
    gameplay::launch_ball(airborne_ball, 7, 0, 0);
    check(airborne_ball.state == 1,
          "A stationary ball above the launch height threshold must remain airborne");

    gameplay::BallFlightState stopped_ball;
    stopped_ball.y = -28.0f;
    gameplay::launch_ball(stopped_ball, 7, 0, 0);
    check(stopped_ball.state == 0,
          "A zero-speed ground launch must enter the stopped state");

    gameplay::BallFlightState capped_ball;
    capped_ball.y = -28.0f;
    gameplay::launch_ball(capped_ball, 0, 42000, 0, 0);
    check(capped_ball.speed == 0x9fff,
          "PES launch speed above its 135-percent threshold must clamp to 0x9fff");

    check(gameplay::kick_power_adjustment(0) == 3553 &&
          gameplay::kick_power_adjustment(49) == 2048 &&
          gameplay::kick_power_adjustment(99) == 512 &&
          gameplay::kick_power_adjustment(127) == 512,
          "Guarded PES attribute adjustment or its 0..99 cap changed");
}

void captured_vertical_position_steps() {
    // These integer velocities are derived from the captured Y deltas and the
    // 2560-unit state scale. The dumps do not expose the temporary velocity
    // field itself, so this verifies the recovered equation against the
    // observed output grid without claiming a full landing/bounce match.
    struct Case {
        float before_y;
        std::int32_t vertical_velocity;
        float after_y;
    };
    constexpr Case cases[] = {
        {-32.41796875f, 1131, -28.0f},
        {-52.37109375f, 1303, -47.28125f},
        {-348.109375f, 1001, -344.19921875f},
    };
    for (const auto& test : cases) {
        gameplay::BallFlightState ball;
        ball.y = test.before_y;
        ball.vertical_velocity = test.vertical_velocity;
        ball.state = 1;
        gameplay::step_ball_flight_vertical_position(ball, {10});
        check(ball.y == test.after_y && ball.state == 1,
              "Recovered airborne Y step missed a captured position delta");
    }
}
}  // namespace

int main() {
    try {
        captured_state1_steps();
        original_kick_launch_fields();
        captured_vertical_position_steps();
        std::cout << checks << " PES ball checks passed across three runtime snapshots and kick setup cases\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
