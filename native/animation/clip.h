#pragma once
#include "skeleton.h"
#include "../assets/mesh.h"
#include <string>

namespace animation {
constexpr std::size_t max_clip_bytes = 32*1024*1024;
// PEVA0001 is our local viewer format, not a game file format.
struct Clip {
    Skeleton skeleton;
    std::vector<SkinVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<std::vector<LocalPose>> samples;
    std::uint32_t fps;
    std::array<float,3> centre;
    float radius;
    std::vector<Vec3> positions(std::size_t sample) const;
    assets::Mesh mesh(std::size_t sample) const;
};
Clip decode_clip(const std::vector<std::uint8_t>& bytes);
Clip load_clip(const std::string& path);

// Accumulate foreground playing time only. No interpolation or automatic loop.
class Playback {
public:
    Playback(std::size_t samples, std::uint32_t fps);
    void advance(std::uint64_t elapsed_ns);
    void rewind() { elapsed_ = 0; }
    std::size_t sample() const;
    bool ended() const { return sample() == samples_-1; }
private:
    std::size_t samples_;
    std::uint32_t fps_;
    std::uint64_t elapsed_ = 0;
};
}
