#pragma once

#include "../assets/ktmdl.h"
#include <array>
#include <vector>

namespace skinning {
using Matrix = std::array<float, 16>; // Column-major GLES matrix.
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Bounds { Vec3 centre; float radius = 0; };
struct Instance {
    std::vector<Matrix> bones; // One skin matrix per skeleton bone.
    Matrix transform = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    // World placement after centering/scaling the pose.
};

// Converts the three float4 rows written by PES to the GLES matrix layout used
// with glUniformMatrix4fv(..., GL_FALSE) and mat4 * vec4 in the vertex shader.
Matrix from_d3d_rows(const std::array<float, 12>& rows);
std::vector<Matrix> packet_palette(const assets::SkinPacket& packet,
                                   const std::vector<Matrix>& bones);
Vec3 deform_position(const assets::SkinPacket& packet, std::size_t vertex,
                     const std::vector<Matrix>& bones);
Bounds bounds(const assets::SkinnedModel& model, const std::vector<Matrix>& bones);
} // namespace skinning
