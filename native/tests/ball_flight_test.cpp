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

void captured_rio_pass_step() {
    // The pass dump stores pre-step position/angle but not the incoming speed.
    // 18292 is the integer speed that best matches the recorded X/Z delta.
    gameplay::BallFlightState ball{
        -8166.50390625f, -32.41796875f, 1476.345703125f,
        16826, 191, 18292, 0
    };
    gameplay::step_ball_flight_horizontal(ball, {10});

    check(ball.angle == 16815, "PES spin and position jitter changed the wrong heading");
    check(std::abs(ball.z - 1404.962890625f) < 0.004f,
          "Recovered flight step missed the captured forward position");
    check(std::abs(ball.x - (-8169.3984375f)) < 0.04f,
          "Recovered flight step missed the captured lateral position");
    check(ball.y == -32.41796875f && ball.speed == 18292,
          "Horizontal step must leave vertical/contact and speed state untouched");
}
}  // namespace

int main() {
    try {
        captured_rio_pass_step();
        std::cout << checks << " original ball-flight checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
