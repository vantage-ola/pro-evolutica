#include "skinning.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace skinning {
namespace {
const assets::SkinAttribute* find(const assets::SkinPacket& packet, std::uint8_t semantic) {
    const auto it = std::find_if(packet.attributes.begin(), packet.attributes.end(),
        [semantic](const auto& attribute) { return attribute.semantic == semantic; });
    return it == packet.attributes.end() ? nullptr : &*it;
}
float read_float(const std::uint8_t* bytes) {
    float result;
    std::memcpy(&result, bytes, sizeof(result));
    if (!std::isfinite(result)) throw std::runtime_error("Non-finite skinned vertex value");
    return result;
}
Vec3 apply(const Matrix& m, Vec3 p) {
    return {m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12],
            m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13],
            m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]};
}
} // namespace

Matrix from_d3d_rows(const std::array<float, 12>& rows) {
    Matrix result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            result[column * 4 + row] = rows[row * 4 + column];
    result[15] = 1.f;
    return result;
}

std::vector<Matrix> packet_palette(const assets::SkinPacket& packet,
                                   const std::vector<Matrix>& bones) {
    if (packet.palette.empty() || packet.palette.size() > 32)
        throw std::runtime_error("Invalid skinned packet palette size");
    std::vector<Matrix> local;
    local.reserve(packet.palette.size());
    for (const auto bone : packet.palette) {
        if (bone >= bones.size()) throw std::runtime_error("Skin matrix missing for packet bone");
        local.push_back(bones[bone]);
    }
    return local;
}

Vec3 deform_position(const assets::SkinPacket& packet, std::size_t vertex,
                     const std::vector<Matrix>& bones) {
    if (vertex >= packet.vertex_count ||
        packet.vertices.size() != std::size_t(packet.vertex_count) * packet.vertex_stride)
        throw std::runtime_error("Invalid skinned vertex range");
    const auto* position = find(packet, 16);
    const auto* bone_ids = find(packet, 33);
    const auto* weight = find(packet, 32);
    if (!position || position->format != 2 || !bone_ids || bone_ids->format != 11 ||
        (weight && weight->format > 2))
        throw std::runtime_error("Packet lacks supported skin attributes");
    const auto* source = packet.vertices.data() + vertex * packet.vertex_stride;
    const auto* p = source + position->offset;
    const Vec3 point{read_float(p), read_float(p + 4), read_float(p + 8)};
    const auto* ids = source + bone_ids->offset;
    const std::size_t explicit_count = weight ? std::size_t(weight->format) + 1 : 0;
    float explicit_weights[3]{};
    float residual = 1.f;
    for (std::size_t i = 0; i < explicit_count; ++i) {
        explicit_weights[i] = read_float(source + weight->offset + i * sizeof(float));
        if (explicit_weights[i] < 0.f || explicit_weights[i] > 1.f)
            throw std::runtime_error("Skin weight outside 0..1");
        residual -= explicit_weights[i];
    }
    if (residual < -1e-5f) throw std::runtime_error("Skin weights sum above 1");
    residual = std::max(0.f, residual);
    Vec3 result{};
    for (std::size_t i = 0; i <= explicit_count; ++i) {
        const float amount = i == 0 ? residual : explicit_weights[i - 1];
        if (amount == 0.f) continue;
        const auto slot = ids[i];
        if (slot >= packet.palette.size())
            throw std::runtime_error("Vertex references an absent packet palette slot");
        const auto bone = packet.palette[slot];
        if (bone >= bones.size()) throw std::runtime_error("Skin matrix missing for referenced packet bone");
        const auto transformed = apply(bones[bone], point);
        result.x += transformed.x * amount;
        result.y += transformed.y * amount;
        result.z += transformed.z * amount;
    }
    return result;
}

Bounds bounds(const assets::SkinnedModel& model, const std::vector<Matrix>& bones) {
    if (model.packets.empty()) throw std::runtime_error("Skinned model has no packets");
    Vec3 low{}, high{};
    bool first = true;
    for (const auto& packet : model.packets) {
        for (std::size_t vertex = 0; vertex < packet.vertex_count; ++vertex) {
            const auto point = deform_position(packet, vertex, bones);
            if (first) { low = high = point; first = false; }
            else {
                low.x = std::min(low.x, point.x); high.x = std::max(high.x, point.x);
                low.y = std::min(low.y, point.y); high.y = std::max(high.y, point.y);
                low.z = std::min(low.z, point.z); high.z = std::max(high.z, point.z);
            }
        }
    }
    if (first) throw std::runtime_error("Skinned model has no vertices");
    Bounds result{{(low.x + high.x) * .5f, (low.y + high.y) * .5f,
                   (low.z + high.z) * .5f}, 0.f};
    for (const auto& packet : model.packets) {
        for (std::size_t vertex = 0; vertex < packet.vertex_count; ++vertex) {
            const auto point = deform_position(packet, vertex, bones);
            const float x = point.x - result.centre.x, y = point.y - result.centre.y,
                        z = point.z - result.centre.z;
            result.radius = std::max(result.radius, std::sqrt(x*x + y*y + z*z));
        }
    }
    if (!std::isfinite(result.radius) || result.radius <= 1e-6f)
        throw std::runtime_error("Skinned model has negligible posed bounds");
    return result;
}
} // namespace skinning
