#include "../rendering/cube_data.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(float a, float b) { return std::abs(a - b) < .0001f; }
std::array<float, 4> project(const cube_data::Matrix& m, float x, float y, float z) {
    std::array<float, 4> result{};
    const float point[] = {x, y, z, 1};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            result[row] += m[column * 4 + row] * point[column];
    return result;
}

void geometry() {
    for (std::size_t i = 0; i < cube_data::indices.size(); i += 3) {
        for (int k = 0; k < 3; ++k)
            check(cube_data::indices[i + k] < cube_data::vertices.size(), "Index out of bounds");
        const auto a = cube_data::vertices[cube_data::indices[i]];
        const auto b = cube_data::vertices[cube_data::indices[i + 1]];
        const auto c = cube_data::vertices[cube_data::indices[i + 2]];
        const float ux = b.x-a.x, uy = b.y-a.y, uz = b.z-a.z;
        const float vx = c.x-a.x, vy = c.y-a.y, vz = c.z-a.z;
        const float nx = uy*vz-uz*vy, ny = uz*vx-ux*vz, nz = ux*vy-uy*vx;
        check(close(nx*nx + ny*ny + nz*nz, 16), "Degenerate or incorrectly sized face");
        check(nx*a.x + ny*a.y + nz*a.z > 0, "Triangle winding is inward");
    }
    for (int face = 0; face < 6; ++face) {
        unsigned corners = 0;
        for (int k = 0; k < 4; ++k) {
            const auto vertex = cube_data::vertices[face * 4 + k];
            check(std::abs(vertex.x) == 1 && std::abs(vertex.y) == 1 &&
                  std::abs(vertex.z) == 1, "Invalid cube corner");
            check((vertex.u == 0 || vertex.u == 1) &&
                  (vertex.v == 0 || vertex.v == 1), "Invalid texture coordinate");
            corners |= 1u << (unsigned(vertex.u) + 2 * unsigned(vertex.v));
            check(vertex.light > 0 && vertex.light <= 1, "Invalid face shade");
        }
        check(corners == 15, "Face is missing a texture corner");
    }
}

void texture() {
    const auto pixels = cube_data::checkerboard();
    int light = 0, dark = 0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const std::size_t offset = (y * 64 + x) * 4;
            check(pixels[offset + 3] == 255, "Checkerboard must be opaque");
            if (pixels[offset] == 230) {
                check(pixels[offset + 1] == 235 && pixels[offset + 2] == 245, "Wrong light colour");
                ++light;
            } else {
                check(pixels[offset] == 35 && pixels[offset + 1] == 105 &&
                      pixels[offset + 2] == 175, "Wrong dark colour");
                ++dark;
            }
        }
    }
    check(light == 2048 && dark == 2048, "Unbalanced checkerboard");
    check(pixels[0] == 230 && pixels[7*4] == 230 && pixels[8*4] == 35,
          "Wrong horizontal tile boundary");
    check(pixels[7*64*4] == 230 && pixels[8*64*4] == 35 &&
          pixels[(8*64 + 8)*4] == 230, "Wrong vertical tile boundary");
}

void projection() {
    const auto square = cube_data::transform(0, 1);
    const auto origin = project(square, 0, 0, 0);
    check(close(origin[0], 0) && close(origin[1], 0) && close(origin[3], 4.5f),
          "Camera should centre the cube at distance 4.5");
    check(origin[2] / origin[3] > -1 && origin[2] / origin[3] < 1, "Wrong depth range");
    const auto right = project(square, 1, 0, 0);
    check(close(right[0] / right[3], .38490018f) && close(right[1], 0),
          "Wrong field of view, handedness or matrix layout");
    const auto portrait = project(cube_data::transform(0, .5f), 1, 0, 0);
    const auto landscape = project(cube_data::transform(0, 2), 1, 0, 0);
    check(close(portrait[0] / portrait[3], .38490018f), "Portrait width is not preserved");
    check(close(landscape[0] / landscape[3], .19245009f), "Landscape aspect is not applied");
    const auto quarter_turn = project(cube_data::transform(1.57079632679f, 1), 1, 0, 0);
    check(close(quarter_turn[0], 0) && close(quarter_turn[3], 5.5f),
          "Quarter turn should move the right corner away from the camera");
    const auto full_turn = cube_data::transform(6.28318530718f, 1);
    for (std::size_t i = 0; i < square.size(); ++i)
        check(close(square[i], full_turn[i]), "Full rotation does not return to start");
}

void visibility() {
    for (float aspect : {.25f, .5f, 1.0f, 2.0f, 4.0f})
        for (int step = 0; step < 72; ++step) {
            const auto matrix = cube_data::transform(step * .0872664626f, aspect);
            for (const auto vertex : cube_data::vertices) {
                const auto point = project(matrix, vertex.x, vertex.y, vertex.z);
                check(point[3] > 0, "Corner is behind the camera");
                for (int axis = 0; axis < 3; ++axis)
                    check(std::isfinite(point[axis]) &&
                          std::abs(point[axis] / point[3]) < 1, "Rotating cube is clipped");
            }
        }
}

void invalid_inputs() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float aspect : {0.0f, -1.0f, nan, infinity}) {
        bool rejected = false;
        try { cube_data::transform(0, aspect); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid aspect was accepted");
    }
    for (float angle : {nan, infinity}) {
        bool rejected = false;
        try { cube_data::transform(angle, 1); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid angle was accepted");
    }
}
} // namespace

int main() {
    try {
        geometry();
        texture();
        projection();
        visibility();
        invalid_inputs();
        std::cout << "5 cube checks passed: geometry, texture, projection, visibility, invalid inputs\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
