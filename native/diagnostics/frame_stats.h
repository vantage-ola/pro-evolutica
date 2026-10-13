#pragma once
#include <cstdint>
#include <vector>

namespace diagnostics {
struct Frame {
    std::uint64_t end_ns, interval_ns, submit_ns, swap_ns;
};
struct Distribution { double mean_ms, p50_ms, p95_ms, p99_ms, max_ms; };
struct Summary {
    std::size_t frames;
    double seconds, fps;
    std::size_t over_16_67_ms, over_33_33_ms, over_50_ms;
    Distribution interval, submit, swap;
};
Summary summarise(const std::vector<Frame>& frames);

class Capture {
public:
    static constexpr std::size_t max_frames = 20'000;
    void begin(std::uint64_t now_ns, std::uint64_t warmup_ns, std::uint64_t duration_ns);
    // Returns true once the requested recording duration has been reached.
    bool frame(std::uint64_t now_ns, std::uint64_t submit_ns, std::uint64_t swap_ns);
    void cancel();
    bool active() const { return active_; }
    const std::vector<Frame>& frames() const { return frames_; }
private:
    std::vector<Frame> frames_;
    bool active_ = false, recording_ = false;
    std::uint64_t begin_ = 0, warmup_ = 0, duration_ = 0, last_ = 0, recorded_ = 0;
};
} // namespace diagnostics
