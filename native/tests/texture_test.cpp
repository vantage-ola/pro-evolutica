#include "../assets/texture.h"
#include "../assets/bundle.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
int checks = 0;
void require(bool value) {
    if (!value) throw std::runtime_error("Texture assertion failed");
    ++checks;
}
void reject(const std::function<void()>& action) {
    try { action(); } catch (const std::runtime_error&) { ++checks; return; }
    throw std::runtime_error("Unsupported texture accepted");
}
void put32(Bytes& b, std::size_t p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.at(p + i) = v >> (i * 8);
}
Bytes fixture(unsigned w = 4, unsigned h = 4, unsigned mips = 1) {
    std::size_t length = 128;
    for (unsigned level = 0; level < mips; ++level)
        length += ((std::max(1u, w >> level) + 3) / 4) *
                  ((std::max(1u, h >> level) + 3) / 4) * 8;
    Bytes b(48 + length);
    put32(b, 0, 2); put32(b, 4, 8);
    // Empty first entry: selection must remain explicit. Third fields are opaque.
    put32(b, 16, 0xfffffff0); put32(b, 20, 32); put32(b, 24, 16 + length);
    put32(b, 28, 0xfffffff0); put32(b, 32, 0x30304557);
    b[39] = 0xff; put32(b, 40, length);
    const auto d = 48;
    put32(b, d, 0x20534444); put32(b, d + 4, 124);
    put32(b, d + 12, h); put32(b, d + 16, w); put32(b, d + 28, mips);
    put32(b, d + 76, 32); put32(b, d + 80, 4); put32(b, d + 84, 0x31545844);
    for (std::size_t p = d + 128; p < b.size(); p += 8) {
        put32(b, p, 0x0000f800); // red, black
        put32(b, p + 4, 0xe4e4e4e4); // selectors 0,1,2,3 in each row
    }
    return b;
}
Bytes dxt5_fixture() {
    auto b = fixture();
    b.resize(b.size() + 8);
    std::move_backward(b.begin() + 176, b.begin() + 184, b.begin() + 192);
    put32(b, 24, 16 + 144); put32(b, 40, 144); put32(b, 132, 0x35545844);
    b[176] = 255; b[177] = 0;
    b[178] = 0x88; b[179] = 0x06; // alpha selectors 0,1,2,3 on the first row
    return b;
}
void changed(std::size_t offset, std::uint32_t value) {
    auto b = fixture(); put32(b, offset, value);
    reject([&] { assets::decode_bundle_texture(b, 1); });
}
} // namespace

int main() {
    try {
        auto b = fixture();
        const auto selected = assets::bundle_entry(b, 1);
        require(selected.offset == 32 && selected.size == b.size()-32);
        require(assets::bundle_entry(b, 0).size == 0);
        // A range is format-neutral; a model record needs no WE00 marker.
        auto model_bundle = b;
        put32(model_bundle, 32, 0x444d544b);
        require(assets::bundle_entry(model_bundle, 1).size == selected.size);
        // Validate the unselected record too, even after selecting a valid one.
        put32(model_bundle, 20, 0xffffffff);
        reject([&] { assets::bundle_entry(model_bundle, 0); });
        auto image = assets::decode_bundle_texture(b, 1);
        require(image.width == 4 && image.height == 4 && image.rgba.size() == 64);
        require(Bytes(image.rgba.begin(), image.rgba.begin() + 16) ==
                Bytes({255,0,0,255, 0,0,0,255, 170,0,0,255, 85,0,0,255}));
        image = assets::decode_bundle_texture(dxt5_fixture(), 1);
        require(image.rgba.size() == 64 && image.rgba[3] == 255 && image.rgba[7] == 0 &&
                image.rgba[11] == 218 && image.rgba[15] == 182);
        auto dxt5 = dxt5_fixture();
        dxt5[176] = 0; dxt5[177] = 255; dxt5[178] = 0xbe; // selectors 6, 7, 2
        image = assets::decode_bundle_texture(dxt5, 1);
        require(image.rgba[3] == 0 && image.rgba[7] == 255 && image.rgba[11] == 51);
        // Deliberately exercise rounding: 16 and 8 interpolate to 13 and 11.
        put32(b, 176, 0x08001000);
        image = assets::decode_bundle_texture(b, 1);
        require(image.rgba[8] == 13 && image.rgba[12] == 11);
        put32(b, 176, 0xf8000000); // three-colour mode and transparent selector 3
        image = assets::decode_bundle_texture(b, 1);
        require(image.rgba[8] == 127 && image.rgba[11] == 255);
        require(Bytes(image.rgba.begin() + 12, image.rgba.begin() + 16) == Bytes({0,0,0,0}));
        put32(b, 176, 0x00000000); // equal endpoints also use transparency
        require(assets::decode_bundle_texture(b, 1).rgba[15] == 0);
        b = fixture(5, 3, 3);
        image = assets::decode_bundle_texture(b, 1);
        require(image.width == 5 && image.height == 3 && image.rgba.size() == 60);
        require(image.rgba[16] == 255 && image.rgba[19] == 255); // second block
        b = fixture(1, 1); put32(b, 76, 0); // zero mip count means one
        require(assets::decode_bundle_texture(b, 1).rgba == Bytes({255,0,0,255}));
        b.resize(b.size() + 15); put32(b, 24, b.size() - 32);
        require(assets::decode_bundle_texture(b, 1).rgba.size() == 4);
        b.back() = 1; reject([&] { assets::decode_bundle_texture(b, 1); });
        b.back() = 0; b.push_back(0); put32(b, 24, b.size() - 32);
        reject([&] { assets::decode_bundle_texture(b, 1); });
        b = fixture();
        reject([&] { assets::decode_bundle_texture(b, 0); });
        reject([&] { assets::decode_bundle_texture(b, 2); });
        reject([&] { assets::decode_bundle_texture(b, std::size_t(-1)); });
        // Every truncated prefix must fail, including complete header/partial mip data.
        for (std::size_t n = 0; n < b.size(); ++n) {
            Bytes short_b(b.begin(), b.begin() + n);
            reject([&] { assets::decode_bundle_texture(short_b, 1); });
        }
        changed(0, 0); changed(0, 4097); changed(4, 12);
        changed(8, 0xffffffff); changed(12, 1); // invalid unselected record
        changed(20, 16); changed(20, 0xfffffff0); changed(24, 0xffffffff);
        changed(32, 0); changed(36, 0x08010000); // wrong type and format
        changed(40, 127); changed(40, 0xffffffff); changed(48, 0);
        changed(52, 123); changed(60, 0); changed(64, 4097);
        auto large = fixture(); put32(large, 60, 4096); put32(large, 64, 4096);
        reject([&] { assets::decode_bundle_texture(large, 1); }); // >4M pixels
        changed(72, 2); changed(76, 4); changed(124, 0);
        changed(128, 0); changed(132, 0x35545844); changed(160, 0x200);
        b.resize(64 * 1024 * 1024 + 1);
        reject([&] { assets::decode_bundle_texture(b, 1); });
        std::cout << checks << " texture checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
