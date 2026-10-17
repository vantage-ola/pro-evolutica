#include "ball_flight.h"

#include <array>
#include <cmath>
#include <cstdint>

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
}  // namespace

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

}  // namespace gameplay
