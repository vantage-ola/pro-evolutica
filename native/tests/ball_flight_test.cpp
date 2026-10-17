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
}  // namespace

int main() {
    try {
        captured_state1_steps();
        std::cout << checks << " original ball-flight checks passed across three runtime snapshots\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
