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

struct SkinAttribute {
    std::uint8_t stream = 0, offset = 0, format = 0, semantic = 0;
};
struct SkinBone {
    std::uint64_t name_id = 0;
    std::int32_t parent = -1;
    std::array<float, 16> rest_global{}, inverse_bind{};
};
struct SkinPacket {
    std::uint8_t primitive = 0;
    std::uint16_t material_index = 0;
    std::vector<std::uint16_t> palette;
    std::vector<TextureReference> textures;
    std::vector<SkinAttribute> attributes;
    std::uint16_t vertex_count = 0, vertex_stride = 0;
    std::vector<std::uint8_t> vertices;
    std::vector<std::uint16_t> indices;
};
struct SkinnedModel {
    std::vector<SkinBone> bones;
    std::vector<SkinPacket> packets;
};

// Standalone little-endian KTMDL 2.1/2.2: one static triangle-strip packet,
// one vertex/index stream, float3 positions/normals and float2 UV0. Supports
// no bones or one identity root. Keeps source units, winding and texture IDs.
// Mesh retains UV0 only; texture references may identify other source UV sets.
// No skinning, morphs, shader recreation, debug-name parsing or bundle pairing.
// Throws on malformed/unsupported input; never executes model data.
StaticModel decode_static_ktmdl(const std::vector<std::uint8_t>& bytes);
// Bounded KTMDL 2.2 skinned triangle-list packets with one vertex/index stream.
// Retains vertex bytes and packet palettes for the Android skinning renderer,
// including float1..float3 weights and UBYTE4 bone indices.
SkinnedModel decode_skinned_ktmdl(const std::vector<std::uint8_t>& bytes);
} // namespace assets
