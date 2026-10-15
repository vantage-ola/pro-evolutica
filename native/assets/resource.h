#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace assets {
constexpr std::uint64_t max_afs_bytes = 8ULL * 1024 * 1024 * 1024;
constexpr std::uint32_t max_afs_entries = 100'000;
constexpr std::size_t max_resource_bytes = 64 * 1024 * 1024;

struct Resource {
    std::uint32_t id = 0;
    std::uint32_t entry_count = 0, entry_offset = 0, entry_size = 0;
    std::vector<std::uint8_t> bytes;
};

// Observed stored (00 01 00) and zlib (00 01 01) WESYS wrappers only.
// Throws on unsupported, malformed or oversized input; never executes payloads.
std::vector<std::uint8_t> decode_wesys(const std::vector<std::uint8_t>& bytes);

// Base archives only: ID high byte 0x0b–0x0e selects dt0b.img–dt0e.img;
// lower 24 bits select the entry. Reads its actual length, excluding sector padding.
// img_dir must contain stable regular files. No alternate-provider/loose-file lookup.
Resource load_resource(const std::string& img_dir, std::uint32_t id);
} // namespace assets
