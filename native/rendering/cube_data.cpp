#include "cube_data.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cube_data {
Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
    return result;
}

Pixels checkerboard() {
    Pixels pixels{};
    for (int y = 0; y < texture_size; ++y) {
        for (int x = 0; x < texture_size; ++x) {
            const bool light = ((x / 8) + (y / 8)) % 2 == 0;
            const std::size_t offset = (y * texture_size + x) * 4;
            pixels[offset] = light ? 230 : 35;
            pixels[offset + 1] = light ? 235 : 105;
            pixels[offset + 2] = light ? 245 : 175;
            pixels[offset + 3] = 255;
        }
    }
    return pixels;
}

Matrix rotation(float angle, float tilt_angle) {
    if (!std::isfinite(angle) || !std::isfinite(tilt_angle)) throw std::invalid_argument("Rotation angle must be finite");
    const float cy = std::cos(angle), sy = std::sin(angle);
    const float cx = std::cos(tilt_angle), sx = std::sin(tilt_angle);
    const Matrix yaw{cy,0,-sy,0, 0,1,0,0, sy,0,cy,0, 0,0,0,1};
    const Matrix tilt{1,0,0,0, 0,cx,sx,0, 0,-sx,cx,0, 0,0,0,1};
    return multiply(yaw, tilt);
}

Matrix transform(float angle, float aspect) {
    if (!std::isfinite(angle) || !std::isfinite(aspect) || aspect <= 0)
        throw std::invalid_argument("Cube angle/aspect must be finite; aspect must be positive");
    const float distance = 4.5f * std::max(1.0f, 1.0f / aspect);
    const Matrix view{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-distance,1};
    constexpr float near = .1f, far = 100.0f, focal = 1.73205080757f;
    const Matrix projection{focal/aspect,0,0,0, 0,focal,0,0,
        0,0,(far+near)/(near-far),-1, 0,0,2*far*near/(near-far),0};
    return multiply(projection, multiply(view, rotation(angle)));
}
} // namespace cube_data
