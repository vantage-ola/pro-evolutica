#include "bundle.h"

#include <stdexcept>

namespace assets {
BundleEntry bundle_entry(const std::vector<std::uint8_t>& bytes, std::size_t index) {
    if (bytes.size() > 64 * 1024 * 1024 || bytes.size() < 8)
        throw std::runtime_error("Invalid numeric bundle size");
    const auto u32 = [&](std::size_t p) {
        return std::uint32_t(bytes[p]) | (std::uint32_t(bytes[p+1]) << 8) |
            (std::uint32_t(bytes[p+2]) << 16) | (std::uint32_t(bytes[p+3]) << 24);
    };
    const auto count = u32(0), table = u32(4);
    if (!count || count > 4096 || table != 8 || index >= count)
        throw std::runtime_error("Unsupported numeric bundle table or index");
    const std::size_t end = 8 + std::size_t(count) * 12;
    if (end > bytes.size()) throw std::runtime_error("Truncated numeric bundle table");
    BundleEntry selected{};
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = u32(8+i*12), size = u32(12+i*12);
        if (offset > bytes.size() || size > bytes.size()-offset || (size && offset < end))
            throw std::runtime_error("Numeric bundle entry is out of bounds");
        if (i == index) selected = {offset,size};
    }
    return selected;
}
} // namespace assets
