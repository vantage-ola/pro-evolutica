#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace assets {
constexpr std::size_t max_obj_bytes = 32 * 1024 * 1024;
constexpr std::size_t max_mesh_vertices = 65'535;
constexpr std::size_t max_mesh_indices = 600'000;

struct MeshVertex { float x, y, z, u, v, nx, ny, nz; };
struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::array<float, 3> centre{};
    float radius = 0;
};

// The static export subset: v/vt/vn arrays of equal size, triangle faces whose
// three positive indices agree, at most one object, no materials/external files.
// Source positions and UVs are retained; centre/radius are framing metadata.
Mesh decode_obj(std::string_view text);
Mesh load_obj(const std::string& path);
} // namespace assets
