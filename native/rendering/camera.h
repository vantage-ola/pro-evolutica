#pragma once
#include "cube_data.h"
#include <array>
#include <cstdint>

namespace camera {
struct View {
    float yaw = 0, pitch = 0, zoom = 1;
    void orbit(float horizontal, float vertical);
    void dolly(float amount); // Logarithmic distance change; negative moves closer.
    void reset();
    cube_data::Matrix transform(float angle, float aspect, float radius) const;
};
float stick(std::int16_t value);
void gamepad(View& view, std::int16_t x, std::int16_t y, std::int16_t zoom, float seconds);

enum class Action { none, tap, reset, orbit, zoom };
// Two tracked fingers. Coordinates are pixels so drag and pinch work in either orientation.
class Touch {
public:
    void down(std::int64_t id, float x, float y, double seconds);
    void move(std::int64_t id, float x, float y, float viewport, View& view);
    Action up(std::int64_t id, double seconds);
    void cancel();
private:
    struct Finger { std::int64_t id = 0; float x = 0, y = 0; bool active = false; };
    std::array<Finger, 2> fingers_{};
    float start_x_ = 0, start_y_ = 0;
    double start_time_ = 0;
    Action action_ = Action::none;
};
} // namespace camera
