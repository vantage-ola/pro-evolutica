#include "../assets/png.h"

#include <zlib.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using Bytes = std::vector<std::uint8_t>;
int checks = 0;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}

void reject(const std::function<void()>& action) {
    try { action(); }
    catch (const std::exception&) { ++checks; return; }
    throw std::runtime_error("Malformed/unsupported PNG was accepted");
}

void put32(Bytes& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(value >> shift);
}

Bytes header(std::uint32_t width = 3, std::uint32_t height = 5) {
    Bytes bytes;
    put32(bytes, width); put32(bytes, height);
    bytes.insert(bytes.end(), {8,6,0,0,0});
    return bytes;
}

Bytes compress(const Bytes& bytes) {
    uLongf size = compressBound(bytes.size());
    Bytes result(size);
    if (compress2(result.data(), &size, bytes.data(), bytes.size(), 9) != Z_OK)
        throw std::runtime_error("Fixture compression failed");
    result.resize(size);
    return result;
}

void chunk(Bytes& png, const std::string& type, const Bytes& payload) {
    put32(png, payload.size());
    const auto start = png.size();
    png.insert(png.end(), type.begin(), type.end());
    png.insert(png.end(), payload.begin(), payload.end());
    const auto crc = crc32(0, png.data() + start, static_cast<uInt>(payload.size() + 4));
    put32(png, crc);
}

Bytes signature() { return {137,80,78,71,13,10,26,10}; }

Bytes png(const Bytes& ihdr, const Bytes& compressed) {
    auto result = signature();
    chunk(result, "IHDR", ihdr);
    chunk(result, "IDAT", compressed);
    chunk(result, "IEND", {});
    return result;
}

// Original 3x5 image with mixed alpha and byte wraparound. Each row uses the
// next filter type. Fixed filtered bytes avoid sharing a predictor with the reader.
const Bytes pixels = {
    0,73,146,219,91,164,237,54,182,255,72,145,
    57,130,203,20,148,221,38,111,239,56,129,202,
    114,187,4,77,205,22,95,168,40,113,186,3,
    171,244,61,134,6,79,152,225,97,170,243,60,
    228,45,118,191,63,136,209,26,154,227,44,117
};
const Bytes filtered = {
    0,0,73,146,219,91,164,237,54,182,255,72,145,
    1,57,130,203,20,91,91,91,91,91,91,91,91,
    2,57,57,57,57,57,57,57,57,57,57,57,57,
    3,114,151,59,96,74,202,74,74,74,74,74,202,
    4,57,57,57,57,57,91,57,57,57,57,57,91
};

void decoding() {
    const auto compressed = compress(filtered);
    const auto image = assets::decode_png(png(header(), compressed));
    require(image.width == 3 && image.height == 5, "Incorrect PNG dimensions");
    require(image.rgba == pixels, "Filters/alpha/row order changed pixels");
    // Separate IDAT chunks are one zlib stream; optional metadata is skipped.
    auto split = signature();
    chunk(split, "IHDR", header());
    chunk(split, "tEXt", {'k',0,'v'});
    chunk(split, "IDAT", {});
    for (auto byte : compressed) chunk(split, "IDAT", {byte});
    chunk(split, "tEXt", {'x',0,'y'});
    chunk(split, "IEND", {});
    require(assets::decode_png(split).rgba == pixels, "Split IDAT changed pixels");
    // First-row/first-pixel edge cases for all predictors, with no previous row.
    const Bytes first_rows[] = {
        {0,0,73,146,219,91,164,237,54,182,255,72,145},
        {1,0,73,146,219,91,91,91,91,91,91,91,91},
        {2,0,73,146,219,91,164,237,54,182,255,72,145},
        {3,0,73,146,219,91,128,164,201,137,173,210,118},
        {4,0,73,146,219,91,91,91,91,91,91,91,91}
    };
    const Bytes first(pixels.begin(), pixels.begin() + 12);
    for (const auto& row : first_rows)
        require(assets::decode_png(png(header(3,1), compress(row))).rgba == first,
                "First-row predictor failed");
    const auto limit = assets::decode_png(png(header(4096,1), compress(Bytes(16385,0))));
    require(limit.rgba.size() == 16384, "Dimension limit boundary rejected");
}

void malformed() {
    const auto compressed = compress(filtered);
    const auto valid = png(header(), compressed);
    for (std::size_t size = 0; size < valid.size(); ++size)
        reject([&] { assets::decode_png(Bytes(valid.begin(), valid.begin() + size)); });
    auto corrupt = valid;
    corrupt[0] ^= 1;
    reject([&] { assets::decode_png(corrupt); });
    corrupt = valid; corrupt[29] ^= 1; // IHDR CRC.
    reject([&] { assets::decode_png(corrupt); });
    corrupt = valid; corrupt[33] = 255; // IDAT length.
    reject([&] { assets::decode_png(corrupt); });
    corrupt = valid; corrupt.push_back(0);
    reject([&] { assets::decode_png(corrupt); });
    for (const auto& dimensions : {header(0,1), header(1,0), header(4097,1), header(1,4097),
                                  header(4096,4096), header(0xffffffff,0xffffffff)})
        reject([&] { assets::decode_png(png(dimensions, compressed)); });
    for (int field = 8; field < 13; ++field) {
        auto unsupported = header(); unsupported[field] = field == 8 ? 16 : 1;
        reject([&] { assets::decode_png(png(unsupported, compressed)); });
    }
    for (int colour : {0,2,3,4}) {
        auto unsupported = header(); unsupported[9] = colour;
        reject([&] { assets::decode_png(png(unsupported, compressed)); });
    }
    auto bad_filter = filtered; bad_filter[13] = 5;
    reject([&] { assets::decode_png(png(header(), compress(bad_filter))); });
    for (auto bad_stream : {Bytes{}, Bytes{1,2,3}, Bytes(compressed.begin(), compressed.end()-1)})
        reject([&] { assets::decode_png(png(header(), bad_stream)); });
    auto bad_adler = compressed; bad_adler.back() ^= 1;
    reject([&] { assets::decode_png(png(header(), bad_adler)); });
    auto trailing = compressed; trailing.push_back(0);
    reject([&] { assets::decode_png(png(header(), trailing)); });
    trailing = compressed; trailing.insert(trailing.end(), compressed.begin(), compressed.end());
    reject([&] { assets::decode_png(png(header(), trailing)); });
    reject([&] { assets::decode_png(png(header(1,1), compressed)); }); // Too much output.
    reject([&] { assets::decode_png(png(header(3,6), compressed)); }); // Too little output.
    reject([&] { assets::decode_png(png(header(1,1), compress(Bytes(1024*1024,0)))); });
    reject([&] { assets::decode_png(Bytes(assets::max_png_bytes + 1,0)); });
}

void ordering() {
    const auto compressed = compress(filtered);
    for (const std::string name : {"IHDR","PLTE","ABCD","abcd","acTL","fcTL","fdAT"}) {
        auto bytes = signature();
        chunk(bytes, "IHDR", header());
        chunk(bytes, name, name == "IHDR" ? header() : Bytes{});
        chunk(bytes, "IDAT", compressed); chunk(bytes, "IEND", {});
        reject([&] { assets::decode_png(bytes); });
    }
    auto bytes = signature(); chunk(bytes, "IDAT", compressed);
    reject([&] { assets::decode_png(bytes); });
    bytes = signature(); chunk(bytes, "IHDR", header()); chunk(bytes, "IEND", {});
    reject([&] { assets::decode_png(bytes); });
    bytes = signature(); chunk(bytes, "IHDR", header()); chunk(bytes, "IDAT", compressed);
    chunk(bytes, "IEND", {0});
    reject([&] { assets::decode_png(bytes); });
    bytes = png(header(), compressed); chunk(bytes, "IEND", {});
    reject([&] { assets::decode_png(bytes); });
    bytes = signature(); chunk(bytes, "IHDR", header()); chunk(bytes, "IDAT", compressed);
    chunk(bytes, "tEXt", {}); chunk(bytes, "IDAT", {}); chunk(bytes, "IEND", {});
    reject([&] { assets::decode_png(bytes); });
    bytes = signature(); chunk(bytes, "IHDR", header());
    for (int i = 0; i < 4096; ++i) chunk(bytes, "tEXt", {});
    chunk(bytes, "IDAT", compressed); chunk(bytes, "IEND", {});
    reject([&] { assets::decode_png(bytes); });
}

void files() {
    char path[] = "/tmp/pro-evolutica-png-XXXXXX";
    if (!mkdtemp(path)) throw std::runtime_error("Cannot create fixture directory");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{path};
    const std::string directory(path), file = directory + "/image.png";
    const auto bytes = png(header(), compress(filtered));
    { std::ofstream output(file, std::ios::binary);
      output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      if (!output) throw std::runtime_error("Cannot write fixture"); }
    require(assets::load_png(file).rgba == pixels, "File decode differs from memory decode");
    reject([&] { assets::load_png(directory + "/missing"); });
    reject([&] { assets::load_png(directory); });
    const auto link = directory + "/link";
    require(!symlink(file.c_str(), link.c_str()), "Cannot create symlink fixture");
    reject([&] { assets::load_png(link); });
    const auto fifo = directory + "/fifo";
    require(!mkfifo(fifo.c_str(), 0600), "Cannot create FIFO fixture");
    reject([&] { assets::load_png(fifo); });
    const int fd = open(file.c_str(), O_WRONLY);
    if (fd < 0) throw std::runtime_error("Cannot open oversized fixture");
    const int status = ftruncate(fd, assets::max_png_bytes + 1);
    close(fd);
    require(!status, "Cannot resize fixture");
    reject([&] { assets::load_png(file); });
}
} // namespace

int main() {
    try {
        decoding(); malformed(); ordering(); files();
        std::cout << checks << " PNG checks passed: pixels, filters, CRC, bounds, zlib, ordering, files\n";
    } catch (const std::exception& error) {
        std::cerr << "PNG test failed: " << error.what() << '\n';
        return 1;
    }
}
