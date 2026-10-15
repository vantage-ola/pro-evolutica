#pragma once

#include "mesh.h"

namespace assets {
struct TextureReference {
    std::uint16_t binding_index = 0, name_index = 0;
    std::uint8_t mode = 0, parameter = 0, uv_channel = 0;
    std::uint64_t id_hi = 0, id_lo = 0;
    std::array<float, 16> parameters{};
};
struct StaticModel {
    Mesh mesh;
    std::uint16_t material_index = 0;
    std::vector<TextureReference> textures;
};

// Standalone little-endian KTMDL 2.1/2.2: one static triangle-strip packet,
// one vertex/index stream, float3 positions/normals and float2 UV0. Supports
// no bones or one identity root. Keeps source units, winding and texture IDs.
// Mesh retains UV0 only; texture references may identify other source UV sets.
// No skinning, morphs, shader recreation, debug-name parsing or bundle pairing.
// Throws on malformed/unsupported input; never executes model data.
StaticModel decode_static_ktmdl(const std::vector<std::uint8_t>& bytes);
} // namespace assets
