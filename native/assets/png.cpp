#include "png.h"

#include <zlib.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace assets {
namespace {
std::uint32_t big_endian(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}

[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int da = std::abs(p - a), db = std::abs(p - b), dc = std::abs(p - c);
    return da <= db && da <= dc ? a : (db <= dc ? b : c);
}
} // namespace

Image decode_png(const std::vector<std::uint8_t>& bytes) {
    constexpr std::uint8_t signature[] = {137,80,78,71,13,10,26,10};
    if (bytes.size() > max_png_bytes) fail("PNG exceeds 16 MiB file limit");
    if (bytes.size() < 8 || !std::equal(std::begin(signature), std::end(signature), bytes.begin()))
        fail("Invalid PNG signature");
    Image image;
    std::vector<std::uint8_t> compressed;
    bool header = false, data = false, data_finished = false, end = false;
    std::size_t offset = 8, chunks = 0;
    while (offset < bytes.size()) {
        if (++chunks > 4096) fail("PNG exceeds chunk count limit");
        if (bytes.size() - offset < 12) fail("Truncated PNG chunk");
        const std::size_t length = big_endian(bytes.data() + offset);
        if (length > bytes.size() - offset - 12) fail("PNG chunk length exceeds input");
        const auto* type = bytes.data() + offset + 4;
        const auto* payload = type + 4;
        for (int i = 0; i < 4; ++i)
            if (!((type[i] >= 'A' && type[i] <= 'Z') || (type[i] >= 'a' && type[i] <= 'z')))
                fail("Invalid PNG chunk name");
        if (type[2] & 32) fail("Invalid PNG reserved chunk bit");
        if (crc32(0, type, static_cast<uInt>(length + 4)) != big_endian(payload + length))
            fail("PNG chunk CRC mismatch");
        const std::string name(reinterpret_cast<const char*>(type), 4);
        if (!header && name != "IHDR") fail("PNG must start with IHDR");
        if (name == "IHDR") {
            if (header || length != 13) fail("Invalid or duplicate PNG header");
            image.width = big_endian(payload);
            image.height = big_endian(payload + 4);
            if (!image.width || !image.height || image.width > max_image_dimension ||
                image.height > max_image_dimension ||
                std::uint64_t(image.width) * image.height > max_image_pixels)
                fail("PNG dimensions exceed image limits");
            if (payload[8] != 8 || payload[9] != 6 || payload[10] || payload[11] || payload[12])
                fail("Only non-interlaced RGBA8 PNG is supported");
            header = true;
        } else if (name == "IDAT") {
            if (data_finished) fail("PNG IDAT chunks must be consecutive");
            compressed.insert(compressed.end(), payload, payload + length);
            data = true;
        } else {
            if (data) data_finished = true;
            if (name == "IEND") {
                if (!data || length) fail("Invalid PNG end chunk");
                end = true;
                offset += length + 12;
                break;
            }
            if (!(type[0] & 32)) fail("Unsupported PNG critical chunk");
            if (name == "acTL" || name == "fcTL" || name == "fdAT")
                fail("Animated PNG is unsupported");
            // Metadata is CRC-checked but not interpreted (including colour profiles).
        }
        offset += length + 12;
    }
    if (!end || offset != bytes.size()) fail("Missing PNG end or trailing bytes");
    if (compressed.empty()) fail("PNG has no compressed image data");
    const std::size_t stride = std::size_t(image.width) * 4;
    const std::size_t expected = (stride + 1) * image.height;
    // One spare byte detects output longer than the declared image.
    std::vector<std::uint8_t> rows(expected + 1);
    z_stream stream{};
    stream.next_in = compressed.data();
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = rows.data();
    stream.avail_out = static_cast<uInt>(rows.size());
    if (inflateInit(&stream) != Z_OK) fail("Cannot initialise PNG decompression");
    const int status = inflate(&stream, Z_FINISH);
    const bool valid = status == Z_STREAM_END && stream.total_out == expected && !stream.avail_in;
    inflateEnd(&stream);
    if (!valid) fail("PNG zlib data is corrupt or has incorrect length");
    image.rgba.resize(stride * image.height);
    for (std::size_t y = 0; y < image.height; ++y) {
        const auto* source = rows.data() + y * (stride + 1);
        const auto filter = *source++;
        if (filter > 4) fail("Invalid PNG row filter");
        auto* row = image.rgba.data() + y * stride;
        for (std::size_t x = 0; x < stride; ++x) {
            const int left = x >= 4 ? row[x - 4] : 0;
            const int above = y ? image.rgba[(y - 1) * stride + x] : 0;
            const int upper_left = y && x >= 4 ? image.rgba[(y - 1) * stride + x - 4] : 0;
            int predictor = 0;
            switch (filter) {
            case 1: predictor = left; break;
            case 2: predictor = above; break;
            case 3: predictor = (left + above) / 2; break;
            case 4: predictor = paeth(left, above, upper_left); break;
            }
            row[x] = static_cast<std::uint8_t>(source[x] + predictor);
        }
    }
    return image;
}

Image load_png(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "Open PNG");
    struct Close { int fd; ~Close() { close(fd); } } close_file{fd};
    struct stat info{};
    if (fstat(fd, &info)) throw std::system_error(errno, std::generic_category(), "Stat PNG");
    if (!S_ISREG(info.st_mode)) fail("PNG input must be a regular file");
    if (info.st_size < 0 || std::uint64_t(info.st_size) > max_png_bytes)
        fail("PNG exceeds 16 MiB file limit");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(info.st_size));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::system_error(errno, std::generic_category(), "Read PNG");
        if (!count) fail("PNG file was truncated while reading");
        offset += static_cast<std::size_t>(count);
    }
    std::uint8_t extra;
    ssize_t count;
    do { count = read(fd, &extra, 1); } while (count < 0 && errno == EINTR);
    if (count != 0) fail("PNG file changed or failed while reading");
    return decode_png(bytes);
}
} // namespace assets
