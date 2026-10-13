#include "../animation/clip.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace {
void check(bool okay) { if (!okay) throw std::runtime_error("Clip check failed"); }
void append(std::vector<std::uint8_t>& b,std::uint64_t value,std::size_t n) {
    for (std::size_t i=0;i<n;++i) b.push_back(static_cast<std::uint8_t>(value>>(i*8)));
}
void f32(std::vector<std::uint8_t>& b,float x) {
    std::uint32_t bits; std::memcpy(&bits,&x,4);append(b,bits,4);
}
void f64(std::vector<std::uint8_t>& b,double x) {
    std::uint64_t bits; std::memcpy(&bits,&x,8);append(b,bits,8);
}
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> b{'P','E','V','A','0','0','0','1'};
    for (const auto x:{1,3,3,3,30}) append(b,x,4);
    for (const auto x:{1.f,0.f,0.f,4.f}) f32(b,x);
    append(b,0xffffffff,4);
    for (int matrix=0;matrix<2;++matrix) for (int i=0;i<16;++i) f32(b,i%5==0 ? 1.f:0.f);
    for (int vertex=0;vertex<3;++vertex) {
        f32(b,vertex==1 ? 1.f:0.f);f32(b,vertex==2 ? 1.f:0.f);f32(b,0);
        append(b,0,4);f64(b,1);for (int i=0;i<3;++i) f64(b,0);
    }
    for (int i=0;i<3;++i) append(b,i,2);
    for (int sample=0;sample<3;++sample) {
        for (const auto x:{3,0,1,128,0,64}) append(b,x,1);
        append(b,1,1);f32(b,float(sample));f32(b,0);f32(b,0);
    }
    return b;
}
template<class F> void refused(F f) {
    bool caught=false;try { f(); } catch (const std::exception&) { caught=true; }check(caught);
}
void package() {
    auto bytes=fixture();const auto clip=animation::decode_clip(bytes);
    check(clip.fps==30 && clip.samples.size()==3 && clip.indices==std::vector<std::uint16_t>({0,1,2}));
    const auto mesh=clip.mesh(2);
    check(std::abs(mesh.vertices[1].x-3)<1e-4 && std::abs(mesh.vertices[2].y-1)<1e-4);
    check(std::abs(mesh.vertices[0].nz-1)<1e-4 && mesh.radius==4);
    refused([&] { clip.positions(3); });
    auto bad=bytes;bad.pop_back();refused([&] { animation::decode_clip(bad); });
    bad=bytes;bad[12]=255;bad[13]=255;bad[14]=255;bad[15]=255;
    refused([&] { animation::decode_clip(bad); });
    bad=bytes;bad[44+132+144]=3;refused([&] { animation::decode_clip(bad); });
    bad=bytes;bad[bad.size()-14]|=128;refused([&] { animation::decode_clip(bad); }); // Later packed reserved bit.
    bad=bytes;bad[bad.size()-13]=2;refused([&] { animation::decode_clip(bad); });
    bad=bytes;bad[40]=0;bad[41]=0;bad[42]=128;bad[43]=63;
    refused([&] { animation::decode_clip(bad); }); // Envelope cannot clip later samples.
    bad=bytes;bad[44+132+16+6]=240;bad[44+132+16+7]=127;
    refused([&] { animation::decode_clip(bad); }); // Non-finite weight.
}
void timing() {
    animation::Playback p(91,30);
    p.advance(33'333'333);check(p.sample()==0);
    p.advance(1);check(p.sample()==1);
    p.advance(1'466'666'666);check(p.sample()==45);
    check(p.sample()==45); // Paused/background: caller does not advance.
    p.advance(std::numeric_limits<std::uint64_t>::max());check(p.sample()==90 && p.ended());
    p.rewind();check(p.sample()==0 && !p.ended());
    animation::Playback single(1,30);single.advance(100);check(single.sample()==0 && single.ended());
    refused([] { animation::Playback p(0,30); });
}
void files() {
    char name[]="/tmp/pro-evolutica-clip-XXXXXX";
    const int fd=mkstemp(name);check(fd>=0);
    const auto bytes=fixture();check(write(fd,bytes.data(),bytes.size())==static_cast<ssize_t>(bytes.size()));close(fd);
    check(animation::load_clip(name).samples.size()==3);
    const std::string link=std::string(name)+"-link",fifo=std::string(name)+"-fifo";
    check(symlink(name,link.c_str())==0);refused([&] { animation::load_clip(link); });unlink(link.c_str());
    check(mkfifo(fifo.c_str(),0600)==0);refused([&] { animation::load_clip(fifo); });unlink(fifo.c_str());
    unlink(name);
}
}
int main() {
    try { package();timing();files();std::cout<<"clip: package, timing and regular-file checks passed\n";return 0; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
