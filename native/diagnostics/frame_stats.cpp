#include "frame_stats.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace diagnostics {
namespace {
Distribution distribution(std::vector<double> values) {
    std::sort(values.begin(),values.end());
    double sum = 0;
    for (const auto value : values) sum += value;
    const auto percentile = [&](double q) { return values[std::size_t(std::ceil(q*values.size()))-1]; };
    return {sum/values.size(),percentile(.5),percentile(.95),percentile(.99),values.back()};
}
}
Summary summarise(const std::vector<Frame>& frames) {
    if (frames.empty() || frames.size() > Capture::max_frames)
        throw std::invalid_argument("Frame summary needs 1..20000 samples");
    Summary result{};
    result.frames = frames.size();
    std::vector<double> interval, submit, swap;
    interval.reserve(frames.size()); submit.reserve(frames.size()); swap.reserve(frames.size());
    for (const auto& f : frames) {
        if (!f.interval_ns || f.submit_ns > f.interval_ns || f.swap_ns > f.interval_ns-f.submit_ns)
            throw std::invalid_argument("Invalid frame interval/stage times");
        result.seconds += double(f.interval_ns)/1e9;
        interval.push_back(double(f.interval_ns)/1e6);
        submit.push_back(double(f.submit_ns)/1e6);
        swap.push_back(double(f.swap_ns)/1e6);
        result.over_16_67_ms += f.interval_ns > 16'666'667;
        result.over_33_33_ms += f.interval_ns > 33'333'333;
        result.over_50_ms += f.interval_ns > 50'000'000;
    }
    result.fps = result.frames/result.seconds;
    result.interval = distribution(std::move(interval));
    result.submit = distribution(std::move(submit));
    result.swap = distribution(std::move(swap));
    return result;
}
void Capture::begin(std::uint64_t now, std::uint64_t warmup, std::uint64_t duration) {
    if (warmup > 30'000'000'000ULL || !duration || duration > 120'000'000'000ULL)
        throw std::invalid_argument("Capture requires warmup <=30s and duration 0..120s");
    frames_.clear();
    frames_.reserve(max_frames); // Allocate before warmup, never per frame.
    begin_ = last_ = now; warmup_ = warmup; duration_ = duration; recorded_ = 0;
    recording_ = false; active_ = true;
}
bool Capture::frame(std::uint64_t now, std::uint64_t submit, std::uint64_t swap) {
    if (!active_) return false;
    if (now <= last_) { cancel(); throw std::invalid_argument("Frame clock must advance"); }
    if (!recording_) {
        last_ = now;
        if (now-begin_ >= warmup_) recording_ = true;
        return false; // Anchor at a completion after warmup; next frame is first sample.
    }
    const auto interval = now-last_;
    if (submit > interval || swap > interval-submit || frames_.size() == max_frames) {
        cancel(); throw std::invalid_argument("Invalid stage times or frame sample limit reached");
    }
    frames_.push_back({now,interval,submit,swap});
    last_ = now; recorded_ += interval;
    if (recorded_ >= duration_) { active_ = false; return true; }
    return false;
}
void Capture::cancel() { active_ = recording_ = false; frames_.clear(); }
} // namespace diagnostics
