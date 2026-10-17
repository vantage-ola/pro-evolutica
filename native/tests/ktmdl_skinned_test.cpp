#include "../assets/ktmdl.h"
#include "../rendering/skinning.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
int checks = 0;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
void put32(Bytes& b, std::size_t p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.at(p+i) = static_cast<std::uint8_t>(v >> (i*8));
}
void put16(Bytes& b, std::size_t p, std::uint16_t v) {
    b.at(p) = static_cast<std::uint8_t>(v); b.at(p+1) = static_cast<std::uint8_t>(v >> 8);
}
void number(Bytes& b, std::size_t p, float v) {
    std::uint32_t bits; std::memcpy(&bits, &v, 4); put32(b, p, bits);
}
Bytes fixture() {
    Bytes b(1008);
    std::memcpy(b.data(), "KTMDL\0\0\0", 8);
    put32(b,8,2); put32(b,12,2); put16(b,16,1); put32(b,144,b.size());
    for (auto [field,count,offset] : std::array<std::array<unsigned,3>,5>{{
             {{24,1,192}}, {{32,1,368}}, {{40,2,384}}, {{48,4,576}}, {{136,9,704}}}}) {
        put32(b,field,count); put32(b,field+4,offset);
    }
    put32(b,116,240); put32(b,120,768);
    for (auto base : {208u,272u})
        for (unsigned i=0;i<16;++i) number(b,base+i*4,i%5==0?1.f:0.f);
    put32(b,364,0xffffffffu);
    put16(b,368,0);

    for (unsigned i=0;i<2;++i) {
        const auto packet=384+i*96, vertex_info=576+i*64, index_info=vertex_info+32;
        b[packet+2]=1; put32(b,packet+16,1); put32(b,packet+20,vertex_info-packet);
        put32(b,packet+24,1); put32(b,packet+28,368-packet);
        put32(b,packet+32,1); put32(b,packet+36,index_info-packet);

        const unsigned stride=i?36:40, count=i?4:5;
        const unsigned vertex_data=i?894:768, index_data=i?1002:888;
        put32(b,vertex_info,static_cast<std::uint32_t>(vertex_data-vertex_info));
        put32(b,vertex_info+4,3); b[vertex_info+8]=1; b[vertex_info+9]=stride;
        b[vertex_info+10]=count; b[vertex_info+11]=1;
        put32(b,vertex_info+12,(i?724:704)-vertex_info); put32(b,vertex_info+20,0);
        put32(b,index_info,static_cast<std::uint32_t>(index_data-index_info));
        put32(b,index_info+4,3); put32(b,index_info+20,1);
        const std::array<std::array<unsigned,4>,5> attrs{{
            {{0,0,2,16}}, {{0,12,0,32}}, {{0,16,11,33}},
            {{0,20,2,18}}, {{0,32,1,22}}}};
        const std::array<std::array<unsigned,4>,4> attrs36{{
            {{0,0,2,16}}, {{0,12,11,33}}, {{0,16,2,18}}, {{0,28,1,22}}}};
        for (unsigned a=0;a<count;++a) {
            const auto& e=i?attrs36[a]:attrs[a];
            const auto at=(i?724:704)+a*4;
            for (unsigned c=0;c<4;++c) b[at+c]=static_cast<std::uint8_t>(e[c]);
        }
        for (unsigned v=0;v<3;++v) {
            const auto at=vertex_data+v*stride;
            number(b,at,float(v)); number(b,at+4,float(v==1)); number(b,at+8,0);
            if (!i) number(b,at+12,1);
            const auto normal=i?at+16:at+20; number(b,normal+8,1);
            const auto uv=i?at+28:at+32; number(b,uv,float(v==1)); number(b,uv+4,float(v==2));
            if (i) b[at+12]=0; else b[at+16]=0;
            put16(b,index_data+v*2,static_cast<std::uint16_t>(v));
        }
    }
    return b;
}
void rejects(const Bytes& bytes) {
    try { assets::decode_skinned_ktmdl(bytes); }
    catch (const std::runtime_error&) { ++checks; return; }
    throw std::runtime_error("Malformed skinned KTMDL accepted");
}
std::array<float, 16> translated(float x) {
    return {1,0,0,0, 0,1,0,0, 0,0,1,0, x,0,0,1};
}
void skinning_checks() {
    const auto matrix=skinning::from_d3d_rows({0,-1,0,2, 1,0,0,3, 0,0,1,4});
    require(matrix[0]==0 && matrix[4]==-1 && matrix[1]==1 && matrix[12]==2 &&
            matrix[13]==3 && matrix[14]==4 && matrix[15]==1,
            "D3D row palette was not packed for GLES column-major upload");

    assets::SkinPacket packet;
    packet.vertex_count=1; packet.vertex_stride=20; packet.palette={1,0};
    packet.vertices.resize(20);
    number(packet.vertices,0,1.f); number(packet.vertices,4,0.f); number(packet.vertices,8,0.f);
    number(packet.vertices,12,.25f); packet.vertices[16]=0; packet.vertices[17]=1;
    packet.attributes={{0,0,2,16},{0,12,0,32},{0,16,11,33}};
    const std::vector<std::array<float,16>> bones={translated(10),translated(20)};
    const auto local=skinning::packet_palette(packet,bones);
    require(local[0][12]==20 && local[1][12]==10,
            "Packet-local palette does not follow its skeleton map");
    const auto deformed=skinning::deform_position(packet,0,bones);
    require(std::abs(deformed.x-18.5f)<1e-6f && deformed.y==0 && deformed.z==0,
            "PES blend weight must weight palette index 1");
}
} // namespace

int main() {
    try {
        auto bytes=fixture(); const auto original=bytes;
        const auto model=assets::decode_skinned_ktmdl(bytes);
        require(bytes==original,"Decoder changed its input");
        require(model.bones.size()==1 && model.bones[0].parent==-1,"Bone hierarchy changed");
        require(model.packets.size()==2,"Multi-packet model rejected");
        require(model.packets[0].vertex_count==3 && model.packets[0].vertex_stride==40 &&
                model.packets[0].vertices.size()==120,"40-byte packet layout changed");
        require(model.packets[1].vertex_count==3 && model.packets[1].vertex_stride==36 &&
                model.packets[1].vertices.size()==108,"36-byte packet layout changed");
        require(model.packets[0].indices==std::vector<std::uint16_t>({0,1,2}) &&
                model.packets[1].indices==std::vector<std::uint16_t>({0,1,2}),"Packet indices changed");
        require(model.packets[0].palette==std::vector<std::uint16_t>({0}) &&
                model.packets[0].attributes.size()==5 && model.packets[1].attributes.size()==4,
                "Packet palette/declarations changed");
        skinning_checks();
        auto bad=fixture(); put16(bad,888,3); rejects(bad);
        bad=fixture(); put32(bad,364,0); rejects(bad); // cyclic self-parent
        bad=fixture(); bad.resize(1007); put32(bad,144,1007); rejects(bad);
        std::cout << checks << " skinned KTMDL checks passed: bones, mixed packet strides, palettes and bounds\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
