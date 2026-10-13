#include "../diagnostics/frame_stats.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks; if (!condition) throw std::runtime_error(message);
}
bool close(double a,double b) { return std::abs(a-b) < 1e-8; }
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected,"Invalid metrics input accepted");
}
void summaries() {
    std::vector<diagnostics::Frame> frames;
    for (std::uint64_t i=1;i<=4;++i) frames.push_back({i*10'000'000,i*10'000'000,1'000'000,2'000'000});
    const auto s = diagnostics::summarise(frames);
    check(s.frames == 4 && close(s.seconds,.1) && close(s.fps,40),"FPS must be count/total time");
    check(close(s.interval.mean_ms,25) && close(s.interval.p50_ms,20),"Wrong mean/nearest-rank median");
    check(close(s.interval.p95_ms,40) && close(s.interval.p99_ms,40) && close(s.interval.max_ms,40),"Spike lost in percentile/max");
    check(s.over_16_67_ms == 3 && s.over_33_33_ms == 1 && s.over_50_ms == 0,"Wrong threshold counts");
    check(close(s.submit.mean_ms,1) && close(s.swap.mean_ms,2),"Submit/swap stages mixed");
    const auto one = diagnostics::summarise({{60'000'000,60'000'000,0,0}});
    check(close(one.interval.p50_ms,60) && one.over_50_ms == 1,"Single sample or >50ms threshold wrong");
    frames = {{1,16'666'667,0,0},{2,33'333'333,0,0},{3,50'000'000,0,0}};
    const auto thresholds = diagnostics::summarise(frames);
    check(thresholds.over_16_67_ms == 2 && thresholds.over_33_33_ms == 1 && thresholds.over_50_ms == 0,"Exact thresholds counted as greater");
    rejects([]{diagnostics::summarise({});});
    rejects([]{diagnostics::summarise({{0,0,0,0}});});
    rejects([]{diagnostics::summarise({{0,10,11,0}});});
    rejects([]{diagnostics::summarise({{0,10,5,6}});});
    rejects([]{diagnostics::summarise(std::vector<diagnostics::Frame>(diagnostics::Capture::max_frames+1));});
}
void captures() {
    diagnostics::Capture c;
    check(!c.frame(1,0,0),"Idle capture records");
    c.begin(100,100,30);
    c.frame(150,0,0); c.frame(200,0,0);
    check(c.frames().empty() && c.active(),"Warmup became a sample");
    check(!c.frame(210,2,3),"Capture finishes too early");
    check(c.frame(235,2,3),"Capture not finished after duration");
    check(!c.active() && c.frames().size() == 2 && c.frames()[0].interval_ns == 10 &&
          c.frames()[1].interval_ns == 25,"Overshooting sample lost or warmup included");
    check(close(diagnostics::summarise(c.frames()).seconds,35e-9),"Elapsed sample sum incorrect");
    check(!c.frame(240,0,0) && c.frames().size() == 2,"Finished capture appended samples");
    c.begin(1000,0,50); c.frame(1001,0,0); c.frame(1011,0,0); c.cancel();
    check(!c.active() && c.frames().empty(),"Cancellation leaves stale samples");
    c.begin(1000,0,50); rejects([&]{c.frame(999,0,0);});
    check(!c.active(),"Bad clock leaves capture active");
    c.begin(1000,0,50); c.frame(1001,0,0); rejects([&]{c.frame(1011,5,6);});
    check(!c.active() && c.frames().empty(),"Bad stage produces successful output");
    rejects([&]{c.begin(0,0,0);}); rejects([&]{c.begin(0,31'000'000'000ULL,1);});
    rejects([&]{c.begin(0,0,121'000'000'000ULL);});
    c.begin(0,0,100'000'000'000ULL); c.frame(1,0,0);
    for (std::size_t i=0;i<diagnostics::Capture::max_frames;++i) c.frame(i+2,0,0);
    rejects([&]{c.frame(diagnostics::Capture::max_frames+2,0,0);});
    check(!c.active() && c.frames().empty(),"Sample cap does not cancel");
}
}
int main() {
    try { summaries(); captures(); std::cout << checks << " frame metric checks passed: summaries, warmup, clocks, bounds, cancellation\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
