#include "../assets/ktmdl.h"

#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
int checks = 0;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
void put32(Bytes& b, std::size_t p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.at(p+i) = v >> (i*8);
}
void put16(Bytes& b, std::size_t p, std::uint16_t v) { b.at(p) = v; b.at(p+1) = v >> 8; }
void number(Bytes& b, std::size_t p, float v) {
    std::uint32_t bits; std::memcpy(&bits,&v,4); put32(b,p,bits);
}
Bytes fixture() {
    Bytes b(1040);
    std::memcpy(b.data(),"KTMDL",5);
    put32(b,8,2); put32(b,12,2); put16(b,16,1); put32(b,144,b.size());
    const auto table = [&](std::size_t field, unsigned count, unsigned offset) {
        put32(b,field,count); put32(b,field+4,offset);
    };
    table(24,1,192); table(32,1,368); table(40,1,384); table(48,2,480);
    table(72,1,576); table(80,1,752); table(124,1,736); table(136,3,544);
    table(116,136,896);
    for (auto base : {208u,272u})
        for (unsigned i = 0; i < 16; ++i) number(b,base+i*4,i%5 == 0 ? 1.f : 0.f);
    put32(b,364,0xffffffff);
    put32(b,400,1); put32(b,404,96); put32(b,408,1); put32(b,412,std::uint32_t(-16));
    put32(b,416,1); put32(b,420,128); b[428] = 1;
    put32(b,480,416); put32(b,484,4); b[488]=1; b[489]=32; b[490]=3; b[491]=1;
    put32(b,492,64);
    put32(b,512,512); put32(b,516,4); put32(b,532,1);
    b[546]=2; b[547]=16; b[549]=12; b[550]=2; b[551]=18;
    b[553]=24; b[554]=1; b[555]=22;
    put32(b,736,0x12345678); put32(b,740,0x9abcdef0); put32(b,744,2);
    number(b,768,1); number(b,772,1);
    const float p[][2]{{0,0},{2,0},{0,3},{2,3}};
    for (unsigned i = 0; i < 4; ++i) {
        const auto v = 896+i*32;
        number(b,v,p[i][0]); number(b,v+4,p[i][1]); number(b,v+20,1);
        number(b,v+24,i%2); number(b,v+28,i/2);
        put16(b,1024+i*2,i);
    }
    return b;
}
void reject(const Bytes& b) {
    try { assets::decode_static_ktmdl(b); }
    catch (const std::runtime_error&) { ++checks; return; }
    throw std::runtime_error("Malformed/unsupported KTMDL accepted");
}
void mutation(std::size_t p, unsigned v) { auto b=fixture(); put32(b,p,v); reject(b); }
void strip(const std::vector<std::uint16_t>& source, const std::vector<std::uint16_t>& expected) {
    auto b=fixture(); b.resize(1024+source.size()*2); put32(b,144,b.size()); put32(b,116,b.size()-896);
    put32(b,516,source.size());
    for (std::size_t i=0;i<source.size();++i) put16(b,1024+i*2,source[i]);
    require(assets::decode_static_ktmdl(b).mesh.indices == expected,"Strip parity/restart changed");
}
} // namespace

int main() {
    try {
        auto b=fixture(); const auto original=b;
        const auto model=assets::decode_static_ktmdl(b); const auto& mesh=model.mesh;
        require(b==original,"Decoder changed input");
        require(mesh.vertices.size()==4 && mesh.indices==std::vector<std::uint16_t>({0,1,2,2,1,3}),"Wrong geometry");
        require(mesh.vertices[3].x==2 && mesh.vertices[3].y==3 && mesh.vertices[3].u==1 &&
                mesh.vertices[3].v==1 && mesh.vertices[3].nz==1,"Attributes changed");
        require(mesh.centre==std::array<float,3>({1,1.5f,0}) && mesh.radius>1.8027f && mesh.radius<1.8028f,"Wrong framing");
        require(model.material_index==0 && model.textures.size()==1,"Wrong material metadata");
        const auto& t=model.textures[0];
        require(t.id_hi==0x9abcdef012345678ULL && t.id_lo==2 && t.mode==0 && t.uv_channel==0 &&
                t.name_index==0 && t.binding_index==0 && t.parameters[0]==1 && t.parameters[1]==1,"Texture ID truncated or metadata changed");
        // The reader uses numeric IDs, not optional debug filenames.
        put32(b,96,0xffffffff); require(assets::decode_static_ktmdl(b).textures[0].id_hi==t.id_hi,"Debug names became required");
        b=fixture(); put32(b,12,1); put32(b,24,0); put32(b,408,0);
        require(assets::decode_static_ktmdl(b).mesh.indices==mesh.indices,"2.1/no-bone packet rejected");
        b=fixture(); b[428]=0; put32(b,72,0); put32(b,80,0); put32(b,124,0);
        require(assets::decode_static_ktmdl(b).textures.empty(),"Plain untextured packet rejected");
        strip({0,0,1,2},{1,0,2});
        strip({0,1,2,2,1,3},{0,1,2,1,2,3});
        strip({0,1,2,65535,2,1,3},{0,1,2,2,1,3});
        for (std::size_t n : {0u,8u,191u,192u,543u,751u,895u,1023u,1029u,1039u}) {
            auto truncated=fixture(); truncated.resize(n);
            if (n >= 192 && n < 1032) put32(truncated,144,n);
            reject(truncated);
        }
        for (const auto& change : std::vector<std::pair<std::size_t,unsigned>>{
                {0,0},{8,3},{12,3},{16,0},{144,1032},{104,1},{132,1},{148,1},
                {24,2},{28,0xffffffff},{40,2},{48,3},{76,896},{80,4097},{128,737},
                {384,0x10000},{400,2},{404,97},{420,96},{412,0},{364,0},
                {480,0x80000000},{484,65536},{516,200001},{492,0xfffffffe},
                {388,1},{388,65536},{432,1},{544,0x20100200},{544,0x20020000}}) mutation(change.first,change.second);
        b=fixture(); b[428]=9; reject(b);
        b=fixture(); put16(b,766,1); reject(b); // bad texture ID index
        b=fixture(); b[435]=1; reject(b); // unavailable UV1
        b=fixture(); put16(b,368,1); reject(b);
        b=fixture(); put16(b,1024,4); reject(b);
        b=fixture(); for (unsigned i=0;i<4;++i) put16(b,1024+i*2,0); reject(b);
        b=fixture(); number(b,896+2*32,1); number(b,900+2*32,0); reject(b);
        b=fixture(); number(b,916,0); reject(b); // zero normal
        for (auto p : {208u,768u,896u,920u}) {
            b=fixture(); number(b,p,std::numeric_limits<float>::quiet_NaN()); reject(b);
        }
        b=fixture(); number(b,208,2); reject(b);
        b=fixture(); b[491]=2; reject(b); // instanced stream
        b=fixture(); b[521]=4; reject(b); // unsupported index stream metadata
        b=fixture(); b[547]=32; reject(b); // skinned semantic
        b.resize(64*1024*1024+1); reject(b);
        std::cout << checks << " KTMDL checks passed: geometry, strips, materials, bounds, unsupported data\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
