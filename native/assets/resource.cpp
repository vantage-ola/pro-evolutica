#include "resource.h"

#include <zlib.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace assets {
namespace {
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

std::uint32_t little_endian(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

void read_exact(int fd, std::uint8_t* destination, std::size_t size, std::uint64_t offset) {
    while (size) {
        const auto count = pread(fd, destination, size, static_cast<off_t>(offset));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::system_error(errno, std::generic_category(), "Read AFS");
        if (!count) fail("Truncated AFS data");
        destination += count;
        size -= static_cast<std::size_t>(count);
        offset += static_cast<std::size_t>(count);
    }
}

bool changed(const struct stat& a, const struct stat& b) {
#ifdef __APPLE__
    return a.st_size != b.st_size || a.st_mtimespec.tv_sec != b.st_mtimespec.tv_sec ||
        a.st_mtimespec.tv_nsec != b.st_mtimespec.tv_nsec || a.st_ctimespec.tv_sec != b.st_ctimespec.tv_sec ||
        a.st_ctimespec.tv_nsec != b.st_ctimespec.tv_nsec;
#else
    return a.st_size != b.st_size || a.st_mtim.tv_sec != b.st_mtim.tv_sec ||
        a.st_mtim.tv_nsec != b.st_mtim.tv_nsec || a.st_ctim.tv_sec != b.st_ctim.tv_sec ||
        a.st_ctim.tv_nsec != b.st_ctim.tv_nsec;
#endif
}
} // namespace

std::vector<std::uint8_t> decode_wesys(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() > max_resource_bytes) fail("WESYS input exceeds 64 MiB limit");
    if (bytes.size() < 16) fail("Truncated WESYS header");
    if (std::memcmp(bytes.data() + 3, "WESYS", 5)) fail("Not a WESYS payload");
    if (bytes[0] != 0 || bytes[1] != 1 || bytes[2] > 1) fail("Unsupported WESYS version");
    const bool stored = bytes[2] == 0;
    const std::size_t payload_size = little_endian(bytes.data() + 8);
    const std::size_t size_field = little_endian(bytes.data() + 12);
    if (stored && size_field) fail("Stored WESYS requires zero at offset 12");
    if (payload_size > max_resource_bytes - 16) fail("WESYS payload exceeds input limit");
    const auto decoded_size = stored ? payload_size : size_field;
    if (decoded_size > max_resource_bytes) fail("WESYS decoded size exceeds 64 MiB limit");
    const auto end = 16 + payload_size;
    if (end > bytes.size()) fail("Truncated WESYS payload");
    if (bytes.size() - end > 15 ||
        std::any_of(bytes.begin() + end, bytes.end(), [](auto byte) { return byte != 0; }))
        fail("WESYS accepts only up to 15 trailing zero padding bytes");
    if (stored) return {bytes.begin() + 16, bytes.begin() + end};

    // One spare byte detects expansion beyond a false small size declaration.
    std::vector<std::uint8_t> decoded(decoded_size + 1);
    z_stream stream{};
    stream.next_in = const_cast<std::uint8_t*>(bytes.data() + 16);
    stream.avail_in = static_cast<uInt>(payload_size);
    stream.next_out = decoded.data();
    stream.avail_out = static_cast<uInt>(decoded.size());
    if (inflateInit(&stream) != Z_OK) fail("Cannot initialise WESYS decompression");
    const int status = inflate(&stream, Z_FINISH);
    const bool valid = status == Z_STREAM_END && stream.total_out == decoded_size && !stream.avail_in;
    inflateEnd(&stream);
    if (!valid) fail("WESYS zlib stream is corrupt or has incorrect length");
    decoded.resize(decoded_size);
    return decoded;
}

Resource load_resource(const std::string& img_dir, std::uint32_t id) {
    constexpr const char* archives[] = {"dt0b.img", "dt0c.img", "dt0d.img", "dt0e.img"};
    const auto selector = id >> 24;
    if (selector < 0x0b || selector > 0x0e) fail("Unsupported resource archive selector");
    if (img_dir.find('\0') != std::string::npos) fail("Invalid archive directory");
    const std::string path = (img_dir.empty() ? "" : img_dir + "/") + archives[selector - 0x0b];
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "Open resource archive");
    struct Close { int fd; ~Close() { close(fd); } } close_file{fd};
    struct stat before{};
    if (fstat(fd, &before)) throw std::system_error(errno, std::generic_category(), "Stat AFS");
    if (!S_ISREG(before.st_mode)) fail("AFS input must be a regular file");
    if (before.st_size < 0 || std::uint64_t(before.st_size) > max_afs_bytes)
        fail("AFS input exceeds 8 GiB limit");
    const auto file_size = static_cast<std::uint64_t>(before.st_size);
    std::uint8_t header[8];
    read_exact(fd, header, sizeof(header), 0);
    if (std::memcmp(header, "AFS\0", 4)) fail("Not an AFS archive");
    const auto count = little_endian(header + 4);
    if (count > max_afs_entries) fail("AFS exceeds entry count limit");
    const std::size_t table_size = std::size_t(count) * 8;
    const auto table_end = 8 + table_size;
    if (table_end > file_size) fail("Truncated AFS entry table");
    std::vector<std::uint8_t> table(table_size);
    read_exact(fd, table.data(), table.size(), 8);
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = little_endian(table.data() + i * 8);
        const auto size = little_endian(table.data() + i * 8 + 4);
        if (std::uint64_t(offset) + size > file_size) fail("AFS entry extends beyond archive");
        if (size && offset < table_end) fail("AFS entry overlaps header or table");
    }
    const auto entry = id & 0xffffff;
    if (entry >= count) fail("Resource entry is outside archive");
    Resource result;
    result.id = id;
    result.entry_count = count;
    result.entry_offset = little_endian(table.data() + std::size_t(entry) * 8);
    result.entry_size = little_endian(table.data() + std::size_t(entry) * 8 + 4);
    if (!result.entry_size) fail("Resource entry is an empty slot");
    if (result.entry_size > max_resource_bytes) fail("Resource entry exceeds 64 MiB limit");
    std::vector<std::uint8_t> raw(result.entry_size);
    read_exact(fd, raw.data(), raw.size(), result.entry_offset);
    struct stat after{};
    if (fstat(fd, &after)) throw std::system_error(errno, std::generic_category(), "Stat AFS after reading");
    if (changed(before, after)) fail("AFS changed while reading; retry with a stable file");
    result.bytes = decode_wesys(raw);
    return result;
}
} // namespace assets
