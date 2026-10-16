#include "ktmdl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace assets {
namespace {
constexpr std::size_t header_size = 192, input_limit = 64 * 1024 * 1024;
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    void range(std::size_t p, std::size_t n) const {
        if (p > bytes.size() || n > bytes.size() - p) fail("KTMDL range outside input");
    }
    void body(std::size_t p, std::size_t n) const {
        if (p < header_size) fail("KTMDL region overlaps header");
        range(p, n);
    }
    std::uint8_t u8(std::size_t p) const { range(p, 1); return bytes[p]; }
    std::uint16_t u16(std::size_t p) const {
        range(p, 2); return std::uint16_t(bytes[p]) | (std::uint16_t(bytes[p+1]) << 8);
    }
    std::uint32_t u32(std::size_t p) const {
        range(p, 4);
        return std::uint32_t(bytes[p]) | (std::uint32_t(bytes[p+1]) << 8) |
            (std::uint32_t(bytes[p+2]) << 16) | (std::uint32_t(bytes[p+3]) << 24);
    }
    std::uint64_t u64(std::size_t p) const { return u32(p) | (std::uint64_t(u32(p+4)) << 32); }
    float number(std::size_t p) const {
        const auto bits = u32(p); float value;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) fail("Non-finite KTMDL value");
        return value;
    }
    std::size_t relative(std::size_t base, std::size_t field) const {
        const auto bits = u32(field);
        const auto delta = bits < 0x80000000u ? std::int64_t(bits) : std::int64_t(bits) - 0x100000000LL;
        const auto p = std::int64_t(base) + delta;
        if (p < 0 || std::uint64_t(p) > bytes.size()) fail("KTMDL relative offset outside input");
        return static_cast<std::size_t>(p);
    }
};
struct Table {
    std::size_t count, offset, stride;
    void contains(std::size_t p, std::size_t n) const {
        if (p < offset || (p-offset) % stride || p-offset > count*stride ||
            n > count*stride - (p-offset)) fail("KTMDL reference outside table");
    }
};
} // namespace

StaticModel decode_static_ktmdl(const std::vector<std::uint8_t>& bytes) {
    const Reader r{bytes};
    if (bytes.size() < header_size || bytes.size() > input_limit ||
        std::memcmp(bytes.data(), "KTMDL\0\0\0", 8) || r.u32(8) != 2 ||
        (r.u32(12) != 1 && r.u32(12) != 2) || r.u16(16) != 1 || r.u32(144) != bytes.size())
        fail("Unsupported standalone KTMDL header");
    if (r.u32(104) || r.u32(132) || r.u32(148)) fail("KTMDL morphs/extra records unsupported");
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    const auto table = [&](std::size_t field, std::size_t stride) {
        const Table t{r.u32(field), r.u32(field+4), stride};
        if (t.count > 4096) fail("KTMDL table exceeds record limit");
        if (t.count) {
            r.body(t.offset, t.count*stride);
            ranges.emplace_back(t.offset, t.offset+t.count*stride);
        }
        return t;
    };
    const auto bones = table(24,176), palette = table(32,2), packets = table(40,96), streams = table(48,32);
    table(56,80); table(64,48);
    const auto materials = table(72,160), bindings = table(80,80);
    table(88,48);
    const auto names = table(124,16), elements = table(136,4);
    const std::size_t data_size = r.u32(116), data_base = r.u32(120);
    r.body(data_base, data_size);
    ranges.emplace_back(data_base, data_base+data_size);
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t i = 1; i < ranges.size(); ++i)
        if (ranges[i-1].second > ranges[i].first) fail("KTMDL tables/data overlap");
    if (packets.count != 1 || streams.count != 2 || !elements.count || bones.count > 1)
        fail("Only one static KTMDL packet is supported");
    if (bones.count) {
        for (const auto offset : {16u,80u})
            for (std::size_t i = 0; i < 16; ++i)
                if (r.number(bones.offset+offset+i*4) != (i%5 == 0 ? 1.f : 0.f))
                    fail("Non-identity KTMDL bone unsupported");
        if (r.u32(bones.offset+172) != 0xffffffffu) fail("Parented KTMDL bone unsupported");
    }
    const auto packet = packets.offset;
    if (r.u8(packet+2) != 0 || r.u32(packet+16) != 1 || r.u32(packet+32) != 1)
        fail("Unsupported KTMDL primitive or stream count");
    const auto palette_count = r.u32(packet+24);
    if (palette_count) {
        const auto p = r.relative(packet, packet+28);
        palette.contains(p, 2);
        if (bones.count != 1 || palette_count != 1 || r.u16(p) != 0)
            fail("Unsupported KTMDL bone palette");
    }
    const auto vertex_stream = r.relative(packet, packet+20), index_stream = r.relative(packet, packet+36);
    streams.contains(vertex_stream,32); streams.contains(index_stream,32);
    if (r.u32(vertex_stream+20) != 0 || r.u32(index_stream+20) != 1)
        fail("Unsupported KTMDL stream type");
    const std::size_t vertex_count = r.u32(vertex_stream+4), index_count = r.u32(index_stream+4);
    const std::size_t stride = r.u8(vertex_stream+9), attribute_count = r.u8(vertex_stream+10);
    if (!vertex_count || vertex_count > max_mesh_vertices || !index_count || index_count > 200'000 ||
        !stride || !attribute_count || attribute_count > 36 || r.u8(vertex_stream+8) != 1 ||
        r.u8(vertex_stream+11) != 1 || r.u32(index_stream+8) != 0 || r.u32(index_stream+12) != 0)
        fail("KTMDL stream layout/count unsupported");
    const auto vertex_data = r.relative(vertex_stream,vertex_stream), index_data = r.relative(index_stream,index_stream);
    const auto data_range = [&](std::size_t p, std::size_t n) {
        if (p < data_base || p-data_base > data_size || n > data_size-(p-data_base))
            fail("KTMDL stream outside data region");
    };
    data_range(vertex_data,vertex_count*stride); data_range(index_data,index_count*2);
    const auto declaration = r.relative(vertex_stream,vertex_stream+12);
    elements.contains(declaration,attribute_count*4);
    std::array<int,256> types, offsets;
    types.fill(-1); offsets.fill(-1);
    for (std::size_t i = 0; i < attribute_count; ++i) {
        const auto a = declaration+i*4;
        const auto channel = r.u8(a), offset = r.u8(a+1), kind = r.u8(a+2), semantic = r.u8(a+3);
        const unsigned size = kind <= 2 ? (kind+1)*4 : kind == 11 ? 4 : 0;
        if (channel || !size || offset+size > stride || types[semantic] != -1 || semantic == 32 || semantic == 33)
            fail("Unsupported KTMDL vertex attribute");
        types[semantic] = kind; offsets[semantic] = offset;
        if (kind != 11)
            for (std::size_t v = 0; v < vertex_count; ++v)
                for (unsigned component = 0; component < size/4; ++component)
                    if (std::abs(r.number(vertex_data+v*stride+offset+component*4)) > 1'000'000)
                        fail("Excessive KTMDL vertex attribute");
    }
    if (types[16] != 2 || types[18] != 2 || types[22] != 1)
        fail("KTMDL needs float3 positions/normals and float2 UV0");
    StaticModel model;
    auto& mesh = model.mesh;
    mesh.vertices.reserve(vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i) {
        const auto p = vertex_data+i*stride+offsets[16], n = vertex_data+i*stride+offsets[18], uv = vertex_data+i*stride+offsets[22];
        MeshVertex v{r.number(p),r.number(p+4),r.number(p+8),r.number(uv),r.number(uv+4),
                     r.number(n),r.number(n+4),r.number(n+8)};
        if (std::hypot(double(v.nx),double(v.ny),double(v.nz)) < 1e-6) fail("Negligible KTMDL normal");
        mesh.vertices.push_back(v);
    }
    unsigned length = 0, parity = 0;
    std::uint16_t a = 0, b = 0;
    for (std::size_t i = 0; i < index_count; ++i) {
        const auto c = r.u16(index_data+i*2);
        if (c == 0xffff) { length = 0; parity = 0; continue; }
        if (c >= vertex_count) fail("KTMDL index exceeds vertex count");
        if (length == 0) { a = c; length = 1; continue; }
        if (length == 1) { b = c; length = 2; continue; }
        if (a != b && b != c && a != c) {
            if (mesh.indices.size() > max_mesh_indices-3) fail("KTMDL triangle count exceeds limit");
            const auto& p = mesh.vertices[a]; const auto& q = mesh.vertices[b]; const auto& s = mesh.vertices[c];
            const double ex = double(q.x)-p.x, ey = double(q.y)-p.y, ez = double(q.z)-p.z;
            const double fx = double(s.x)-p.x, fy = double(s.y)-p.y, fz = double(s.z)-p.z;
            if (std::hypot(ey*fz-ez*fy,ez*fx-ex*fz,ex*fy-ey*fx) == 0) fail("Zero-area KTMDL triangle");
            mesh.indices.insert(mesh.indices.end(), {parity ? b : a, parity ? a : b, c});
        }
        parity ^= 1; a = b; b = c;
    }
    if (mesh.indices.empty()) fail("KTMDL has no non-degenerate triangles");
    std::array<float,3> lower{mesh.vertices[0].x,mesh.vertices[0].y,mesh.vertices[0].z}, upper = lower;
    for (const auto& v : mesh.vertices) {
        const float p[]{v.x,v.y,v.z};
        for (int j = 0; j < 3; ++j) { lower[j] = std::min(lower[j],p[j]); upper[j] = std::max(upper[j],p[j]); }
    }
    for (int j = 0; j < 3; ++j) mesh.centre[j] = (lower[j]+upper[j])*.5f;
    double radius = 0;
    for (const auto& v : mesh.vertices)
        radius = std::max(radius,std::hypot(double(v.x)-mesh.centre[0],double(v.y)-mesh.centre[1],double(v.z)-mesh.centre[2]));
    if (radius < 1e-6) fail("Negligible KTMDL bounds");
    mesh.radius = static_cast<float>(radius);
    const auto material_index = r.u32(packet+4);
    if ((materials.count && material_index >= materials.count) || (!materials.count && material_index))
        fail("KTMDL material index outside table");
    model.material_index = static_cast<std::uint16_t>(material_index);
    const auto ref_count = r.u8(packet+44);
    if (ref_count > 8 || (ref_count && !materials.count)) fail("Unsupported KTMDL texture reference count");
    for (unsigned i = 0; i < ref_count; ++i) {
        const auto ref = packet+48+i*4;
        TextureReference t;
        t.binding_index = r.u16(ref); t.parameter = r.u8(ref+2); t.uv_channel = r.u8(ref+3);
        if (t.binding_index >= bindings.count) fail("KTMDL texture binding outside table");
        const auto binding = bindings.offset+t.binding_index*80;
        t.mode = r.u8(binding+13); t.name_index = r.u16(binding+14);
        if (t.name_index >= names.count || 22u+t.uv_channel >= types.size() || types[22+t.uv_channel] != 1)
            fail("KTMDL texture ID/UV reference outside table");
        const auto id = names.offset+t.name_index*16;
        t.id_hi = r.u64(id); t.id_lo = r.u64(id+8);
        for (std::size_t j = 0; j < 16; ++j) t.parameters[j] = r.number(binding+16+j*4);
        model.textures.push_back(t);
    }
    return model;
}

SkinnedModel decode_skinned_ktmdl(const std::vector<std::uint8_t>& bytes) {
    const Reader r{bytes};
    if (bytes.size() < header_size || bytes.size() > input_limit ||
        std::memcmp(bytes.data(), "KTMDL\0\0\0", 8) || r.u32(8) != 2 ||
        r.u32(12) != 2 || r.u16(16) != 1 || r.u32(144) != bytes.size())
        fail("Unsupported skinned KTMDL header");
    if (r.u32(104) || r.u32(132) || r.u32(148))
        fail("Skinned KTMDL morphs/extra records unsupported");

    const Table bones{r.u32(24), r.u32(28), 176};
    const Table packets{r.u32(40), r.u32(44), 96};
    const Table streams{r.u32(48), r.u32(52), 32};
    const Table materials{r.u32(72), r.u32(76), 160};
    const Table bindings{r.u32(80), r.u32(84), 80};
    const Table names{r.u32(124), r.u32(128), 16};
    const Table elements{r.u32(136), r.u32(140), 4};
    const auto check_table = [&](const Table& table) {
        if (table.count > 4096) fail("KTMDL table exceeds record limit");
        if (table.count) r.body(table.offset, table.count * table.stride);
    };
    check_table(bones); check_table(packets); check_table(streams); check_table(materials);
    check_table(bindings); check_table(names); check_table(elements);
    if (!bones.count || bones.count > 128 || !packets.count || packets.count > 4096 ||
        !streams.count || !elements.count)
        fail("Unsupported skinned KTMDL table counts");

    const auto data_size = r.u32(116), data_base = r.u32(120);
    r.body(data_base, data_size);
    const auto in_data = [&](std::size_t p, std::size_t n) {
        if (p < data_base || p - data_base > data_size || n > data_size - (p - data_base))
            fail("Skinned KTMDL stream outside data region");
    };

    SkinnedModel model;
    model.bones.reserve(bones.count);
    std::vector<unsigned char> visit(bones.count);
    for (std::size_t i = 0; i < bones.count; ++i) {
        const auto offset = bones.offset + i * bones.stride;
        SkinBone bone;
        bone.name_id = r.u64(offset);
        for (std::size_t j = 0; j < 16; ++j) {
            bone.rest_global[j] = r.number(offset + 0x10 + j * 4);
            bone.inverse_bind[j] = r.number(offset + 0x50 + j * 4);
        }
        const auto parent = r.u32(offset + 0xac);
        bone.parent = parent == 0xffffffffu ? -1 : static_cast<std::int32_t>(parent);
        if (bone.parent < -1 || bone.parent >= static_cast<std::int32_t>(bones.count))
            fail("Invalid skinned KTMDL bone parent");
        model.bones.push_back(bone);
    }
    const auto visit_bone = [&](auto&& self, std::size_t node) -> void {
        if (visit[node] == 1) fail("Cyclic skinned KTMDL hierarchy");
        if (visit[node] == 2) return;
        visit[node] = 1;
        const auto parent = model.bones[node].parent;
        if (parent >= 0) self(self, static_cast<std::size_t>(parent));
        visit[node] = 2;
    };
    for (std::size_t i = 0; i < bones.count; ++i) visit_bone(visit_bone, i);

    model.packets.reserve(packets.count);
    for (std::size_t i = 0; i < packets.count; ++i) {
        const auto packet = packets.offset + i * packets.stride;
        if (r.u8(packet + 2) != 1 || r.u32(packet + 16) != 1 || r.u32(packet + 32) != 1)
            fail("Only one-stream skinned KTMDL triangle-list packets are supported");
        SkinPacket decoded;
        decoded.primitive = r.u8(packet + 2);
        const auto material = r.u32(packet + 4);
        if (material > 0xffffu) fail("Skinned KTMDL material index is too large");
        if ((materials.count && material >= materials.count) || (!materials.count && material))
            fail("Skinned KTMDL material index outside table");
        decoded.material_index = static_cast<std::uint16_t>(material);
        const auto texture_count = r.u8(packet + 44);
        if (texture_count > 8 || (texture_count && !materials.count))
            fail("Unsupported skinned KTMDL texture reference count");
        for (unsigned j = 0; j < texture_count; ++j) {
            const auto ref = packet + 48 + j * 4;
            TextureReference texture;
            texture.binding_index = r.u16(ref);
            texture.parameter = r.u8(ref + 2);
            texture.uv_channel = r.u8(ref + 3);
            if (texture.binding_index >= bindings.count)
                fail("Skinned KTMDL texture binding outside table");
            const auto binding = bindings.offset + texture.binding_index * bindings.stride;
            texture.mode = r.u8(binding + 13);
            texture.name_index = r.u16(binding + 14);
            if (texture.name_index >= names.count || 22u + texture.uv_channel >= 256)
                fail("Skinned KTMDL texture name/UV outside tables");
            const auto id = names.offset + texture.name_index * names.stride;
            texture.id_hi = r.u64(id);
            texture.id_lo = r.u64(id + 8);
            for (std::size_t k = 0; k < texture.parameters.size(); ++k)
                texture.parameters[k] = r.number(binding + 16 + k * 4);
            decoded.textures.push_back(texture);
        }

        const auto palette_count = r.u32(packet + 24);
        const auto palette_offset = r.relative(packet, packet + 28);
        if (!palette_count || palette_count > bones.count) fail("Invalid skinned KTMDL palette size");
        r.body(palette_offset, palette_count * 2);
        decoded.palette.reserve(palette_count);
        for (std::size_t j = 0; j < palette_count; ++j) {
            const auto bone = r.u16(palette_offset + j * 2);
            if (bone >= bones.count) fail("Skinned KTMDL palette bone outside skeleton");
            decoded.palette.push_back(bone);
        }

        const auto vertex_info = r.relative(packet, packet + 20);
        const auto index_info = r.relative(packet, packet + 36);
        streams.contains(vertex_info, streams.stride);
        streams.contains(index_info, streams.stride);
        if (r.u32(vertex_info + 20) != 0 || r.u32(index_info + 20) != 1 ||
            r.u8(vertex_info + 8) != 1 || r.u8(vertex_info + 11) != 1 ||
            r.u32(index_info + 8) != 0 || r.u32(index_info + 12) != 0)
            fail("Unsupported skinned KTMDL stream metadata");

        const auto vertex_count = r.u32(vertex_info + 4);
        const auto stride = r.u8(vertex_info + 9);
        const auto attribute_count = r.u8(vertex_info + 10);
        const auto index_count = r.u32(index_info + 4);
        if (!vertex_count || vertex_count > max_mesh_vertices || !stride || stride > 255 ||
            !attribute_count || attribute_count > 36 || !index_count ||
            index_count > max_mesh_indices)
            fail("Skinned KTMDL stream count/layout unsupported");
        decoded.vertex_count = static_cast<std::uint16_t>(vertex_count);
        decoded.vertex_stride = stride;
        const auto vertex_data = r.relative(vertex_info, vertex_info);
        const auto index_data = r.relative(index_info, index_info);
        in_data(vertex_data, vertex_count * stride);
        in_data(index_data, index_count * 2);

        const auto element_offset = r.relative(vertex_info, vertex_info + 12);
        elements.contains(element_offset, attribute_count * elements.stride);
        std::array<bool, 256> semantics{};
        bool position = false, normal = false, uv0 = false, blend_indices = false;
        for (std::size_t j = 0; j < attribute_count; ++j) {
            const auto element = element_offset + j * elements.stride;
            SkinAttribute attribute{r.u8(element), r.u8(element + 1),
                                    r.u8(element + 2), r.u8(element + 3)};
            const auto size = attribute.format <= 2 ? (attribute.format + 1u) * 4u :
                              attribute.format == 11 ? 4u : 0u;
            if (attribute.stream != 0 || !size || attribute.offset + size > stride ||
                semantics[attribute.semantic])
                fail("Unsupported skinned KTMDL vertex attribute");
            semantics[attribute.semantic] = true;
            position |= attribute.semantic == 16 && attribute.format == 2;
            normal |= attribute.semantic == 18 && attribute.format == 2;
            uv0 |= attribute.semantic == 22 && attribute.format == 1;
            blend_indices |= attribute.semantic == 33 && attribute.format == 11;
            decoded.attributes.push_back(attribute);
            if (attribute.format != 11)
                for (std::size_t v = 0; v < vertex_count; ++v)
                    for (unsigned component = 0; component < size / 4; ++component)
                        r.number(vertex_data + v * stride + attribute.offset + component * 4);
        }
        if (!position || !normal || !uv0 || !blend_indices)
            fail("Skinned KTMDL packet lacks a required vertex attribute");

        decoded.vertices.assign(bytes.begin() + vertex_data,
                                bytes.begin() + vertex_data + vertex_count * stride);
        decoded.indices.reserve(index_count);
        for (std::size_t j = 0; j < index_count; ++j) {
            const auto index = r.u16(index_data + j * 2);
            if (index >= vertex_count) fail("Skinned KTMDL index outside packet vertices");
            decoded.indices.push_back(index);
        }
        model.packets.push_back(std::move(decoded));
    }
    return model;
}
} // namespace assets
