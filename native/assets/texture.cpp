#include "texture.h"
#include "bundle.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace assets {
namespace {
using Colour = std::array<std::uint8_t, 4>;
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

std::uint32_t u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) fail("Truncated texture field");
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}

Colour rgb565(std::uint16_t value) {
    unsigned r = value >> 11, g = (value >> 5) & 63, b = value & 31;
    return {std::uint8_t((r << 3) | (r >> 2)), std::uint8_t((g << 2) | (g >> 4)),
            std::uint8_t((b << 3) | (b >> 2)), 255};
}
} // namespace

Image decode_bundle_texture(const std::vector<std::uint8_t>& bytes, std::size_t index) {
    const auto entry = bundle_entry(bytes, index);
    const auto offset = entry.offset, size = entry.size;
    if (size < 16 || u32(bytes, offset) != 0x30304557 || bytes[offset + 6] != 0 ||
        bytes[offset + 7] != 0xff) fail("Unsupported WE00 texture header");
    const std::size_t length = u32(bytes, offset + 8);
    if (length > size - 16 || length < 128) fail("Invalid WE00 DDS length");
    const auto pad = size - 16 - length;
    if (pad > 15 || std::any_of(bytes.begin() + offset + 16 + length,
                               bytes.begin() + offset + size, [](auto b) { return b != 0; }))
        fail("Unsupported WE00 trailing data");
    const auto dds = offset + 16;
    const auto format = u32(bytes, dds + 84);
    const bool dxt1 = format == 0x31545844, dxt5 = format == 0x35545844;
    if (u32(bytes, dds) != 0x20534444 || u32(bytes, dds + 4) != 124 ||
        u32(bytes, dds + 76) != 32 || !(u32(bytes, dds + 80) & 4) ||
        (!dxt1 && !dxt5) || u32(bytes, dds + 24) > 1 ||
        u32(bytes, dds + 112) != 0) fail("Only flat DXT1 and DXT5 DDS textures are supported");
    Image image;
    image.height = u32(bytes, dds + 12); image.width = u32(bytes, dds + 16);
    if (!image.width || !image.height || image.width > max_image_dimension ||
        image.height > max_image_dimension ||
        std::uint64_t(image.width) * image.height > max_image_pixels)
        fail("DDS dimensions exceed limits");
    const auto mips = std::max(1u, u32(bytes, dds + 28));
    unsigned max_mips = 0;
    for (auto n = std::max(image.width, image.height); n; n >>= 1) ++max_mips;
    if (mips > max_mips) fail("DDS mip count exceeds dimensions");
    std::size_t expected = 128;
    for (unsigned level = 0; level < mips; ++level) {
        const auto w = std::max(1u, image.width >> level), h = std::max(1u, image.height >> level);
        expected += std::size_t((w + 3) / 4) * ((h + 3) / 4) * (dxt5 ? 16 : 8);
    }
    if (expected != length) fail("DDS mip data length mismatch");
    image.rgba.resize(std::size_t(image.width) * image.height * 4);
    std::size_t block = dds + 128;
    for (unsigned y = 0; y < image.height; y += 4) {
        for (unsigned x = 0; x < image.width; x += 4, block += dxt5 ? 16 : 8) {
            std::array<std::uint8_t, 8> alpha{};
            std::uint64_t alpha_selectors = 0;
            if (dxt5) {
                const auto a0 = bytes[block], a1 = bytes[block + 1];
                alpha[0] = a0; alpha[1] = a1;
                if (a0 > a1) {
                    for (unsigned i = 1; i <= 6; ++i)
                        alpha[i + 1] = std::uint8_t(((7 - i) * a0 + i * a1) / 7);
                } else {
                    for (unsigned i = 1; i <= 4; ++i)
                        alpha[i + 1] = std::uint8_t(((5 - i) * a0 + i * a1) / 5);
                    alpha[6] = 0; alpha[7] = 255;
                }
                for (unsigned i = 0; i < 6; ++i)
                    alpha_selectors |= std::uint64_t(bytes[block + 2 + i]) << (8 * i);
            }
            const auto colour_block = block + (dxt5 ? 8 : 0);
            const auto endpoints = u32(bytes, colour_block), selectors = u32(bytes, colour_block + 4);
            const auto c0 = std::uint16_t(endpoints), c1 = std::uint16_t(endpoints >> 16);
            std::array<Colour, 4> palette = {rgb565(c0), rgb565(c1), Colour{}, Colour{}};
            for (unsigned channel = 0; channel < 3; ++channel) {
                if (dxt5 || c0 > c1) {
                    palette[2][channel] = (2 * palette[0][channel] + palette[1][channel] + 1) / 3;
                    palette[3][channel] = (palette[0][channel] + 2 * palette[1][channel] + 1) / 3;
                } else palette[2][channel] = (palette[0][channel] + palette[1][channel]) / 2;
            }
            palette[2][3] = 255; palette[3][3] = dxt5 || c0 > c1 ? 255 : 0;
            for (unsigned py = 0; py < 4 && y + py < image.height; ++py) {
                for (unsigned px = 0; px < 4 && x + px < image.width; ++px) {
                    auto colour = palette[(selectors >> (2 * (py * 4 + px))) & 3];
                    if (dxt5) colour[3] = alpha[(alpha_selectors >> (3 * (py * 4 + px))) & 7];
                    const auto pixel = (std::size_t(y + py) * image.width + x + px) * 4;
                    std::copy(colour.begin(), colour.end(), image.rgba.begin() + pixel);
                }
            }
        }
    }
    return image;
}
} // namespace assets
