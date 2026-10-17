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
    // Consecutive rows from the game's own 224-record trajectory cache. The
    // inputs include the captured speed, spin, flags, and heading, so no value
    // is fitted from the output position.
    struct Case {
        gameplay::BallFlightState input;
        float expected_x;
        float expected_z;
        std::uint16_t expected_angle;
    };
    constexpr Case cases[] = {
        {{-8169.5703125f, -31.64453125f, 1339.849609375f,
          16414, 191, 16566, 0, -779, 1334, 1},
         -8169.6953125f, 1275.158203125f, 16406},
        {{-9967.1484375f, -41.60546875f, -2232.78125f,
          38057, 156, 16153, 1, 1598, 1345, 1},
         -10022.265625f, -2202.12890625f, 38063},
        {{10816.77734375f, -339.80078125f, 491.1171875f,
          63250, 192, 25671, 0, 1251, -2414, 1},
         10914.62890625f, 512.953125f, 63244},
    };
    for (const auto& test : cases) {
        auto ball = test.input;
        gameplay::step_ball_flight_horizontal(ball, {10});
        check(ball.angle == test.expected_angle,
              "PES spin and position jitter changed the wrong heading");
        check(std::abs(ball.x - test.expected_x) < 0.0001f,
              "Recovered flight step missed a captured lateral position");
        check(std::abs(ball.z - test.expected_z) < 0.0001f,
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
    // These use the recorded incoming vertical-speed field and consecutive
    // output rows from the same three trajectory caches as the horizontal test.
    struct Case {
        float before_y;
        std::int32_t vertical_velocity;
        float after_y;
    };
    constexpr Case cases[] = {
        {-31.64453125f, -779, -34.6875f},
        {-41.60546875f, 1598, -35.36328125f},
        {-339.80078125f, 1251, -334.9140625f},
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

void captured_ground_motion_steps() {
    struct Case {
        gameplay::BallFlightState input;
        std::int32_t final_speed;
        float expected_x;
        float expected_y;
        float expected_z;
        std::uint16_t expected_angle;
        std::int32_t expected_curve;
    };
    constexpr Case cases[] = {
        {{-7991.890625f, -28.0f, -1799.857421875f, 14820, 511, 7569, 0,
          0, 5062, 2},
         7215, -7987.6484375f, -28.0f, -1827.923828125f, 14818, 6879},
        {{-12308.62109375f, -28.0f, -674.1015625f, 40038, 540, 7000, 1,
          0, 4680, 2},
         6674, -12328.75f, -28.0f, -657.25f, 40039, 6360},
        {{17425.46875f, -28.0f, 2384.40234375f, 61332, 576, 6971, 0,
          0, 4660, 2},
         6646, 17449.515625f, -28.0f, 2394.640625f, 61335, 6333},
    };
    for (const auto& test : cases) {
        auto ball = test.input;
        gameplay::step_ball_ground_motion(ball, {10}, test.final_speed);
        check(ball.angle == test.expected_angle && ball.curve == test.expected_curve,
              "PES ground step missed the captured heading or capped curve");
        check(std::abs(ball.x - test.expected_x) < 0.0001f &&
              std::abs(ball.z - test.expected_z) < 0.0001f,
              "PES ground step missed a captured horizontal position");
        check(ball.y == test.expected_y && ball.speed == test.final_speed && ball.spin == 0,
              "Ground step must preserve unresolved Y while applying captured speed/spin");
    }
}
}  // namespace

int main() {
    try {
        captured_state1_steps();
        original_kick_launch_fields();
        captured_vertical_position_steps();
        captured_ground_motion_steps();
        std::cout << checks << " PES ball checks passed across captured airborne and ground steps\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
