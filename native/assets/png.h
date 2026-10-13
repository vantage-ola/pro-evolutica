#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace assets {
constexpr std::size_t max_png_bytes = 16 * 1024 * 1024;
constexpr std::uint32_t max_image_dimension = 4096;
constexpr std::size_t max_image_pixels = 4 * 1024 * 1024;

struct Image {
    std::uint32_t width = 0, height = 0;
    // Top row first, straight (unassociated) RGBA, eight bits per channel.
    std::vector<std::uint8_t> rgba;
};

// Supported subset: non-interlaced RGBA8, PNG filter types 0–4.
// Throws on unsupported, malformed or oversized input.
Image decode_png(const std::vector<std::uint8_t>& bytes);
Image load_png(const std::string& path);
} // namespace assets
