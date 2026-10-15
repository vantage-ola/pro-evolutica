#include "../assets/resource.h"

#include <zlib.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

namespace {
using Bytes = std::vector<std::uint8_t>;
namespace fs = std::filesystem;
int checks = 0;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}

void reject(const std::function<void()>& action) {
    try { action(); }
    catch (const std::runtime_error&) { ++checks; return; }
    throw std::runtime_error("Malformed/unsupported resource was accepted");
}

void put32(Bytes& data, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) data[offset + i] = value >> (i * 8);
}

Bytes wrapped(const Bytes& payload, bool stored, std::size_t padding = 0) {
    Bytes body = payload;
    if (!stored) {
        uLongf size = compressBound(payload.size());
        body.resize(size);
        if (compress2(body.data(), &size, payload.data(), payload.size(), 9) != Z_OK)
            throw std::runtime_error("Fixture compression failed");
        body.resize(size);
    }
    Bytes result = {0, 1, static_cast<std::uint8_t>(stored ? 0 : 1), 'W', 'E', 'S', 'Y', 'S'};
    result.resize(16);
    put32(result, 8, static_cast<std::uint32_t>(body.size()));
    put32(result, 12, stored ? 0 : static_cast<std::uint32_t>(payload.size()));
    result.insert(result.end(), body.begin(), body.end());
    result.resize(result.size() + padding);
    return result;
}

Bytes archive(const Bytes& raw) {
    // Nonzero sector padding and a non-WESYS unselected entry must be ignored.
    Bytes result(4096, 0xa5);
    std::fill(result.begin(), result.begin() + 32, 0);
    result[0] = 'A'; result[1] = 'F'; result[2] = 'S';
    put32(result, 4, 3);
    put32(result, 16, 2048); put32(result, 20, static_cast<std::uint32_t>(raw.size()));
    put32(result, 24, 4092); put32(result, 28, 4);
    std::copy(raw.begin(), raw.end(), result.begin() + 2048);
    return result;
}

void write(const fs::path& path, const Bytes& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void wrappers() {
    const Bytes payload = {'s','y','n','t','h','e','t','i','c',0,255};
    for (bool stored : {false, true}) {
        require(assets::decode_wesys(wrapped(payload, stored, 15)) == payload, "Decoded bytes differ");
        require(assets::decode_wesys(wrapped({}, stored)).empty(), "Empty WESYS failed");
        const auto valid = wrapped(payload, stored);
        for (std::size_t length = 0; length < valid.size(); ++length)
            reject([&] { assets::decode_wesys({valid.begin(), valid.begin() + length}); });
        for (const auto offset : {0U, 1U, 2U, 3U}) {
            auto bad = valid; bad[offset] = 0xff;
            reject([&] { assets::decode_wesys(bad); });
        }
        auto bad = valid; put32(bad, 8, 0xffffffff);
        reject([&] { assets::decode_wesys(bad); });
        bad = valid; put32(bad, 12, 0xffffffff);
        reject([&] { assets::decode_wesys(bad); });
        bad = valid; bad.push_back(1);
        reject([&] { assets::decode_wesys(bad); });
        reject([&] { assets::decode_wesys(wrapped(payload, stored, 16)); });
    }
    auto bad = wrapped(payload, false);
    bad.back() ^= 1;
    reject([&] { assets::decode_wesys(bad); }); // Adler checksum.
    bad = wrapped(payload, false); put32(bad, 12, payload.size() + 1);
    reject([&] { assets::decode_wesys(bad); });
    bad = wrapped(payload, false); bad.push_back(0);
    put32(bad, 8, static_cast<std::uint32_t>(bad.size() - 16));
    reject([&] { assets::decode_wesys(bad); }); // Data inside the declared zlib range.
    bad = wrapped(payload, false);
    const auto second = wrapped(payload, false);
    bad.insert(bad.end(), second.begin() + 16, second.end());
    put32(bad, 8, static_cast<std::uint32_t>(bad.size() - 16));
    reject([&] { assets::decode_wesys(bad); }); // A second zlib stream.
    bad = wrapped(Bytes(2 * 1024 * 1024, 'x'), false); put32(bad, 12, 1);
    reject([&] { assets::decode_wesys(bad); }); // False small output size.
    reject([&] { assets::decode_wesys(Bytes(assets::max_resource_bytes + 1)); });
}

void archives(const fs::path& directory) {
    constexpr const char* names[] = {"dt0b.img", "dt0c.img", "dt0d.img", "dt0e.img"};
    for (std::uint32_t selector = 0x0b; selector <= 0x0e; ++selector) {
        for (bool stored : {false, true}) {
            const Bytes payload = {'r','e','s','o','u','r','c','e', static_cast<std::uint8_t>(selector)};
            const auto raw = wrapped(payload, stored, 3);
            const auto path = directory / names[selector - 0x0b];
            const auto original = archive(raw);
            write(path, original);
            const auto before = fs::last_write_time(path);
            const auto id = (selector << 24) | 1;
            const auto result = assets::load_resource(directory.string(), id);
            require(result.bytes == payload && result.id == id, "Resource selection differs");
            require(result.entry_offset == 2048 && result.entry_size == raw.size() &&
                    result.entry_count == 3, "Entry metadata differs");
            require(before == fs::last_write_time(path) && fs::file_size(path) == original.size(),
                    "Archive metadata changed");
        }
    }
    const auto path = directory / "dt0b.img";
    const auto valid = archive(wrapped({'a','b','c'}, false));
    write(path, valid);
    for (auto id : {0U, 0xffffffffU, 0x0a000001U, 0x0f000001U, 0x0b000000U, 0x0bffffffU})
        reject([&] { assets::load_resource(directory.string(), id); });
    auto bad = valid; bad[0] = 0;
    write(path, bad); reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    for (const auto count : {0U, assets::max_afs_entries + 1}) {
        bad = valid; put32(bad, 4, count);
        write(path, bad); reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    }
    write(path, Bytes(valid.begin(), valid.begin() + 31)); // Truncated index.
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    for (const auto offset : {0U, 31U, 4095U, 0xffffffffU}) {
        bad = valid; put32(bad, 16, offset);
        write(path, bad); reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    }
    bad = valid; put32(bad, 24, 0xffffffff); // Invalid unselected entry.
    write(path, bad); reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    bad = valid; put32(bad, 20, assets::max_resource_bytes + 1);
    write(path, bad); fs::resize_file(path, assets::max_resource_bytes + 4096);
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    fs::resize_file(path, assets::max_afs_bytes + 1); // Sparse file; no large allocation.
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    fs::remove(path);
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    write(directory / "source.img", valid);
    fs::create_symlink(directory / "source.img", path);
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    fs::remove(path); fs::create_directory(path);
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
    fs::remove(path);
    require(mkfifo(path.c_str(), 0600) == 0, "Cannot create FIFO fixture");
    reject([&] { assets::load_resource(directory.string(), 0x0b000001); });
}
} // namespace

int main() {
    char name[] = "/tmp/pro-evolutica-resource-XXXXXX";
    const auto* directory = mkdtemp(name);
    if (!directory) { std::cerr << "Cannot create temporary directory\n"; return 1; }
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); }
    } cleanup{directory};
    try {
        wrappers();
        archives(directory);
        std::cout << checks << " resource checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Resource test failed: " << error.what() << '\n';
        return 1;
    }
}
