#include "camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace camera {
namespace {
constexpr float pi = 3.14159265359f;
void finite(float value) {
    if (!std::isfinite(value)) throw std::invalid_argument("Camera input must be finite");
}
}
void View::orbit(float horizontal, float vertical) {
    finite(horizontal); finite(vertical);
    yaw = std::remainder(yaw + std::clamp(horizontal, -2*pi, 2*pi), 2*pi);
    pitch = std::clamp(pitch + std::clamp(vertical, -pi, pi), -1.45f, 1.45f);
}
void View::dolly(float amount) {
    finite(amount);
    zoom = std::clamp(zoom * std::exp(std::clamp(amount, -4.f, 4.f)), .5f, 3.f);
}
void View::reset() { yaw = pitch = 0; zoom = 1; }

cube_data::Matrix View::transform(float angle, float aspect, float radius) const {
    finite(yaw); finite(pitch); finite(zoom); finite(aspect); finite(radius);
    // Match-sized scenes can have a larger bounding radius than the single
    // inspection model. Keep a generous upper bound to reject invalid inputs
    // without aborting when several captured actors are framed together.
    if (aspect < .1f || aspect > 10 || radius <= 0 || radius > 10 || zoom < .5f || zoom > 3)
        throw std::invalid_argument("Camera bounds/aspect/zoom outside supported range");
    constexpr float focal = 1.73205080757f;
    const float horizontal_focal = focal / aspect;
    const float sphere_fit = radius * std::sqrt(horizontal_focal * horizontal_focal + 1.f);
    const float distance = std::max({4.5f * zoom, 2.1f * radius, sphere_fit});
    const cube_data::Matrix translation{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-distance,1};
    // Inverse orbit: camera yaw then pitch around the centred scene.
    const auto orbit = cube_data::multiply(cube_data::rotation(0, -pitch), cube_data::rotation(-yaw, 0));
    constexpr float near = .1f;
    const float far = distance + radius + 10;
    const cube_data::Matrix projection{focal/aspect,0,0,0, 0,focal,0,0,
        0,0,(far+near)/(near-far),-1, 0,0,2*far*near/(near-far),0};
    return cube_data::multiply(projection, cube_data::multiply(translation,
        cube_data::multiply(orbit, cube_data::rotation(angle))));
}
float stick(std::int16_t value) {
    const float x = value < 0 ? float(value)/32768 : float(value)/32767;
    constexpr float dead = .18f;
    return std::abs(x) <= dead ? 0 : std::copysign((std::abs(x)-dead)/(1-dead), x);
}
void gamepad(View& view, std::int16_t x, std::int16_t y, std::int16_t z, float seconds) {
    finite(seconds);
    if (seconds < 0 || seconds > .1f) throw std::invalid_argument("Camera step outside 0..0.1 seconds");
    view.orbit(stick(x)*seconds*2, stick(y)*seconds*2);
    view.dolly(stick(z)*seconds*1.5f);
}
void Touch::down(std::int64_t id, float x, float y, double seconds) {
    finite(x); finite(y);
    if (!std::isfinite(seconds)) throw std::invalid_argument("Touch time must be finite");
    for (const auto& f : fingers_) if (f.active && f.id == id) return;
    for (auto& f : fingers_) if (!f.active) {
        const bool empty = !fingers_[0].active && !fingers_[1].active;
        if (empty) { start_x_ = x; start_y_ = y; start_time_ = seconds; action_ = Action::none; }
        else action_ = Action::zoom; // Multi-touch can never become a tint tap.
        f = {id,x,y,true};
        return;
    }
    action_ = Action::zoom; // Ignore additional fingers without allowing a tap.
}
void Touch::move(std::int64_t id, float x, float y, float viewport, View& view) {
    finite(x); finite(y); finite(viewport);
    if (viewport <= 0) throw std::invalid_argument("Touch viewport must be positive");
    for (auto& f : fingers_) if (f.active && f.id == id) {
        if (fingers_[0].active && fingers_[1].active) {
            const float before = std::hypot(fingers_[0].x-fingers_[1].x, fingers_[0].y-fingers_[1].y);
            f.x = x; f.y = y;
            const float after = std::hypot(fingers_[0].x-fingers_[1].x, fingers_[0].y-fingers_[1].y);
            if (before >= 8 && after >= 8) view.dolly(std::log(before/after));
            action_ = Action::zoom;
        } else {
            if (action_ == Action::none && std::hypot(x-start_x_, y-start_y_) > 12)
                action_ = Action::orbit;
            if (action_ == Action::orbit) view.orbit((x-f.x)/viewport*2*pi, (y-f.y)/viewport*pi);
            f.x = x; f.y = y;
        }
        return;
    }
}
Action Touch::up(std::int64_t id, double seconds) {
    if (!std::isfinite(seconds)) throw std::invalid_argument("Touch time must be finite");
    for (auto& f : fingers_) if (f.active && f.id == id) {
        f.active = false;
        if (fingers_[0].active || fingers_[1].active) return Action::none;
        const auto result = action_ == Action::none ?
            (seconds-start_time_ >= .6 ? Action::reset : Action::tap) : action_;
        cancel();
        return result;
    }
    return Action::none;
}
void Touch::cancel() { fingers_ = {}; action_ = Action::none; }
} // namespace camera
