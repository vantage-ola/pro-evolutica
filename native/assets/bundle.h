#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace assets {
struct BundleEntry { std::size_t offset, size; };
// Numeric PES bundle table only. Validates every record before selecting one;
// the third record word is opaque. Returned range refers to the unchanged input.
BundleEntry bundle_entry(const std::vector<std::uint8_t>& bytes, std::size_t index);
} // namespace assets
