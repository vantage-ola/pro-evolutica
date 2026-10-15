#pragma once

#include "png.h"

namespace assets {
// Explicit numeric entry in an already decoded PES bundle. Supports WE00 type 0,
// format 0xff containing a flat DXT1 DDS; returns its largest mip as RGBA8.
// Throws on malformed/unsupported data. Does not infer material bindings or names.
Image decode_bundle_texture(const std::vector<std::uint8_t>& bytes, std::size_t index);
} // namespace assets
