#pragma once

#include <array>
#include <cstdint>

namespace cube_data {
struct Vertex {
    float x, y, z, u, v, light;
};

// Four independent vertices per face preserve texture seams and face shading.
inline constexpr std::array<Vertex, 24> vertices = {{
    {-1,-1, 1, 0,0,1}, { 1,-1, 1, 1,0,1}, { 1, 1, 1, 1,1,1}, {-1, 1, 1, 0,1,1},
    { 1,-1,-1, 0,0,.65f}, {-1,-1,-1, 1,0,.65f}, {-1, 1,-1, 1,1,.65f}, { 1, 1,-1, 0,1,.65f},
    { 1,-1, 1, 0,0,.85f}, { 1,-1,-1, 1,0,.85f}, { 1, 1,-1, 1,1,.85f}, { 1, 1, 1, 0,1,.85f},
    {-1,-1,-1, 0,0,.75f}, {-1,-1, 1, 1,0,.75f}, {-1, 1, 1, 1,1,.75f}, {-1, 1,-1, 0,1,.75f},
    {-1, 1, 1, 0,0,1}, { 1, 1, 1, 1,0,1}, { 1, 1,-1, 1,1,1}, {-1, 1,-1, 0,1,1},
    {-1,-1,-1, 0,0,.55f}, { 1,-1,-1, 1,0,.55f}, { 1,-1, 1, 1,1,.55f}, {-1,-1, 1, 0,1,.55f}
}};
inline constexpr std::array<std::uint16_t, 36> indices = {{
    0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11,
    12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
}};
inline constexpr int texture_size = 64;
using Pixels = std::array<std::uint8_t, texture_size * texture_size * 4>;
using Matrix = std::array<float, 16>;

Pixels checkerboard();
// Column-major model/view/projection for a unit cube, with a 60-degree field
// of view and fixed tilt. The camera moves back in portrait to keep it visible.
Matrix transform(float angle, float aspect);
Matrix rotation(float angle);
} // namespace cube_data
