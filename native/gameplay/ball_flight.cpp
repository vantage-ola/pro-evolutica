#include "ball_flight.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace gameplay {
namespace {
constexpr std::int32_t kAngleTurnNumerator = 0x7000;
constexpr std::int32_t kAngleTurnDivisor = 0x8c;
constexpr std::int32_t kAngleCorrectionDivisor = -0x74000000;
constexpr std::int32_t kPositionScale = 2560;

std::int32_t divide_toward_zero(std::int32_t numerator, std::int32_t denominator) {
    return numerator / denominator;
}

std::int32_t shift_toward_zero(std::int32_t value, unsigned bits) {
    const std::int32_t divisor = std::int32_t{1} << bits;
    return value / divisor;
}

std::int32_t arithmetic_shift(std::int32_t value, unsigned bits) {
    if (value >= 0) return value >> bits;
    const auto magnitude = -static_cast<std::int64_t>(value);
    const auto rounded = (magnitude + (std::int64_t{1} << bits) - 1) >> bits;
    return static_cast<std::int32_t>(-rounded);
}

const std::array<std::int32_t, 512>& direction_table() {
    static const std::array<std::int32_t, 512> table = [] {
        std::array<std::int32_t, 512> values{};
        constexpr double tau = 6.283185307179586476925286766559;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const double radians = tau * static_cast<double>(i) / static_cast<double>(values.size());
            // The game's table is trunc(-sin(angle) * 4096), confirmed against
            // all 512 values from its read-only Ghidra memory export.
            values[i] = static_cast<std::int32_t>(-std::sin(radians) * 4096.0);
        }
        return values;
    }();
    return table;
}

std::int32_t interpolate_direction(std::uint16_t angle) {
    const auto& table = direction_table();
    const std::uint32_t index = angle >> 7;
    const std::int32_t fraction = angle & 0x7f;
    const std::int32_t mixed = (0x80 - fraction) * table[index] +
        fraction * table[(index + 1) & 0x1ff];
    return shift_toward_zero(mixed, 7);
}

std::int32_t displacement(std::uint16_t angle, std::int32_t speed, std::int32_t tick) {
    const auto direction = interpolate_direction(angle);
    const auto velocity = shift_toward_zero(direction * speed, 12);
    return velocity * tick;
}

std::int32_t position_integer(float value) {
    return static_cast<std::int32_t>(value);  // PES's captured FTOL mode truncates.
}

float add_internal_displacement(float position, std::int32_t delta) {
    const double scaled = static_cast<double>(position) * kPositionScale + delta;
    return static_cast<float>(scaled / kPositionScale);
}

std::int32_t clamp_i32(std::int64_t value) {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}
}  // namespace

std::int32_t kick_power_adjustment(std::uint8_t attribute) {
    const auto capped = std::min<std::int32_t>(attribute & 0x7f, 99);
    const double value = (1.0 - (static_cast<double>(capped - 49) / 50.0)) * 1536.0 + 512.0;
    return static_cast<std::int32_t>(value);  // Ghidra's FISTP path truncates toward zero.
}

void launch_ball(BallFlightState& ball, std::uint16_t heading,
                 std::int32_t requested_power, std::int32_t vertical_parameter,
                 std::int32_t guarded_power_adjustment) {
    const auto magnitude = requested_power < 0
        ? -static_cast<std::int64_t>(requested_power)
        : static_cast<std::int64_t>(requested_power);
    // FUN_00e20190 derives the initial curve field from the signed request,
    // while FUN_00e1fdb0 receives its absolute speed (plus a guarded bonus).
    ball.curve = requested_power / 4;
    ball.spin = 0;
    auto speed = magnitude + guarded_power_adjustment;
    if (speed < 0) speed = 0;
    if (speed * 100 / 0x7684 > 0x87) speed = 0x9fff;
    ball.speed = clamp_i32(speed);
    ball.angle = heading;
    ball.vertical_velocity = clamp_i32(-static_cast<std::int64_t>(vertical_parameter) * 0xbc);

    if (ball.vertical_velocity != 0 || ball.y < -36.0f) {
        ball.state = 1;
    } else if (ball.speed != 0 || ball.y <= -36.0f) {
        ball.state = 2;
    } else {
        ball.state = 0;
    }
}

void step_ball_flight_horizontal(BallFlightState& ball, BallFlightTick tick) {
    const std::int32_t direction = (ball.direction_flags & 0x0f) != 0 ? 1 : -1;
    if (ball.spin != 0) {
        const std::int32_t adjusted_speed = arithmetic_shift(ball.speed + kAngleTurnNumerator, 1);
        const std::int32_t turn = divide_toward_zero(
            adjusted_speed * tick.delta * direction, ball.spin);
        const std::int32_t angle_delta = divide_toward_zero(turn, kAngleTurnDivisor) -
            divide_toward_zero(turn, kAngleCorrectionDivisor);
        ball.angle = static_cast<std::uint16_t>(ball.angle + angle_delta);
    }

    const std::int32_t jitter_source = position_integer(ball.x) +
        (position_integer(ball.z) - position_integer(ball.y));
    const std::int32_t remainder = jitter_source % 9;
    const std::int32_t jitter = 4 - (remainder < 0 ? -remainder : remainder);
    ball.angle = static_cast<std::uint16_t>(ball.angle + jitter);

    const auto x_angle = static_cast<std::uint16_t>(ball.angle + 0xc000);
    const std::int32_t dx = displacement(x_angle, ball.speed, tick.delta);
    const std::int32_t dz = displacement(ball.angle, ball.speed, tick.delta);
    ball.x = add_internal_displacement(ball.x, dx);
    ball.z = add_internal_displacement(ball.z, dz);
}

void step_ball_flight_vertical_position(BallFlightState& ball, BallFlightTick tick) {
    constexpr double kVerticalPositionScale = 2560.0;
    const double scaled_position = static_cast<double>(ball.y) * kVerticalPositionScale;
    const double vertical_delta = static_cast<double>(tick.delta) * ball.vertical_velocity;
    ball.y = static_cast<float>((scaled_position + vertical_delta) / kVerticalPositionScale);
}

}  // namespace gameplay
