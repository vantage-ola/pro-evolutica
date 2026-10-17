#include "clip.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace animation {
namespace {
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t offset = 0;
    std::uint64_t integer(std::size_t count) {
        if (count > bytes.size()-offset) fail("Truncated animation package");
        std::uint64_t result = 0;
        for (std::size_t i=0;i<count;++i) result |= std::uint64_t(bytes[offset++]) << (8*i);
        return result;
    }
    float f32() {
        const auto bits = static_cast<std::uint32_t>(integer(4));
        float value; std::memcpy(&value,&bits,4);
        if (!std::isfinite(value) || std::abs(value)>1e6f) fail("Invalid animation float");
        return value;
    }
    double f64() {
        const auto bits = integer(8);
        double value; std::memcpy(&value,&bits,8);
        if (!std::isfinite(value)) fail("Invalid animation weight");
        return value;
    }
};
bool changed(const struct stat& a,const struct stat& b) {
#if defined(__APPLE__)
    return a.st_size!=b.st_size || a.st_mtimespec.tv_sec!=b.st_mtimespec.tv_sec ||
        a.st_mtimespec.tv_nsec!=b.st_mtimespec.tv_nsec || a.st_ctimespec.tv_sec!=b.st_ctimespec.tv_sec ||
        a.st_ctimespec.tv_nsec!=b.st_ctimespec.tv_nsec;
#else
    return a.st_size!=b.st_size || a.st_mtim.tv_sec!=b.st_mtim.tv_sec ||
        a.st_mtim.tv_nsec!=b.st_mtim.tv_nsec || a.st_ctim.tv_sec!=b.st_ctim.tv_sec ||
        a.st_ctim.tv_nsec!=b.st_ctim.tv_nsec;
#endif
}
double distance(const Vec3& v,const std::array<float,3>& c) {
    return std::hypot(v[0]-c[0],v[1]-c[1],v[2]-c[2]);
}
Quaternion interpolate(const Quaternion& a,const Quaternion& source_b,double amount) {
    Quaternion b=source_b;
    double dot=0;
    for (std::size_t i=0;i<4;++i) dot+=a[i]*b[i];
    if (dot<0) { dot=-dot; for (double& x:b) x=-x; }
    dot=std::clamp(dot,-1.0,1.0);
    Quaternion out{};
    if (dot>0.9995) {
        double length2=0;
        for (std::size_t i=0;i<4;++i) { out[i]=a[i]+amount*(b[i]-a[i]); length2+=out[i]*out[i]; }
        const double inverse=1/std::sqrt(length2);
        for (double& x:out) x*=inverse;
        return out;
    }
    const double angle=std::acos(dot), sine=std::sin(angle);
    const double wa=std::sin((1-amount)*angle)/sine, wb=std::sin(amount*angle)/sine;
    for (std::size_t i=0;i<4;++i) out[i]=wa*a[i]+wb*b[i];
    return out;
}
assets::Mesh make_mesh(const std::vector<Vec3>& xyz,const std::vector<std::uint16_t>& indices,
                       const std::array<float,3>& centre,float radius) {
    std::vector<Vec3> normals(xyz.size());
    for (std::size_t i=0;i<indices.size();i+=3) {
        const auto a=indices[i],b=indices[i+1],c=indices[i+2];
        Vec3 u{},v{};
        for (std::size_t k=0;k<3;++k) { u[k]=xyz[b][k]-xyz[a][k]; v[k]=xyz[c][k]-xyz[a][k]; }
        const Vec3 n{u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
        for (const auto node:{a,b,c}) for (std::size_t k=0;k<3;++k) normals[node][k]+=n[k];
    }
    assets::Mesh mesh{{},indices,centre,radius};
    mesh.vertices.reserve(xyz.size());
    for (std::size_t i=0;i<xyz.size();++i) {
        const auto length=std::hypot(normals[i][0],normals[i][1],normals[i][2]);
        const auto n=length>1e-12 ? Vec3{normals[i][0]/length,normals[i][1]/length,normals[i][2]/length} : Vec3{0,1,0};
        mesh.vertices.push_back({float(xyz[i][0]),float(xyz[i][1]),float(xyz[i][2]),0,0,float(n[0]),float(n[1]),float(n[2])});
    }
    return mesh;
}
}

Clip decode_clip(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size()<44 || bytes.size()>max_clip_bytes || std::memcmp(bytes.data(),"PEVA0001",8))
        fail("Unsupported animation package header or size");
    Reader r{bytes,8};
    const auto bones=r.integer(4), vertices=r.integer(4), indices=r.integer(4), samples=r.integer(4), fps=r.integer(4);
    if (!bones || bones>max_bones || !vertices || vertices>max_skin_vertices || !indices ||
        indices>assets::max_mesh_indices || indices%3 || !samples || samples>512 ||
        vertices*samples>2'000'000 || !fps || fps>240) fail("Animation package count limit");
    if (44+bones*132+vertices*48+indices*2+samples*bones*19 != bytes.size())
        fail("Animation package byte count mismatch");
    std::array<float,3> centre{};
    for (auto& x:centre) x=r.f32();
    const auto radius=r.f32();
    if (radius<1e-6f) fail("Invalid animation radius");
    std::vector<Bone> bind;
    bind.reserve(bones);
    for (std::size_t i=0;i<bones;++i) {
        Bone b{};
        const auto parent=r.integer(4);
        if (parent != 0xffffffff && parent>=bones) fail("Invalid animation parent");
        b.parent=parent==0xffffffff ? -1 : static_cast<std::int32_t>(parent);
        for (auto& x:b.rest_global) x=r.f32();
        for (auto& x:b.inverse_bind) x=r.f32();
        bind.push_back(b);
    }
    Clip result{Skeleton(std::move(bind)),{},{},{},static_cast<std::uint32_t>(fps),centre,radius};
    result.vertices.resize(vertices);
    for (auto& v:result.vertices) {
        for (auto& x:v.position) x=r.f32();
        for (auto& b:v.bones) b=static_cast<std::uint8_t>(r.integer(1));
        for (auto& w:v.weights) w=r.f64();
    }
    result.indices.resize(indices);
    for (auto& i:result.indices) {
        i=static_cast<std::uint16_t>(r.integer(2));
        if (i>=vertices) fail("Invalid animation triangle index");
    }
    result.samples.resize(samples);
    for (auto& sample:result.samples) {
        sample.resize(bones);
        for (auto& local:sample) {
            std::array<std::uint8_t,6> packed{};
            for (auto& b:packed) b=static_cast<std::uint8_t>(r.integer(1));
            local.rotation=decode_rotation(packed);
            const auto present=r.integer(1);
            if (present>1) fail("Unsupported translation flag");
            Vec3 t{}; for (auto& x:t) x=r.f32();
            if (present) local.translation=t;
            else if (t!=Vec3{}) fail("Unused translation must be zero");
        }
    }
    // Validate the whole clip before publishing it, including later samples and
    // the fixed framing envelope. Work is capped at two million vertex samples.
    for (std::size_t sample=0;sample<samples;++sample)
        for (const auto& v:result.positions(sample))
            if (distance(v,centre)>radius*1.000001) fail("Animation exceeds its framing radius");
    return result;
}

Clip load_clip(const std::string& path) {
    const int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if (fd<0) throw std::system_error(errno,std::generic_category(),"Cannot open animation package");
    struct Close { int fd; ~Close() { close(fd); } } guard{fd};
    struct stat before{},after{};
    if (fstat(fd,&before)) throw std::system_error(errno,std::generic_category(),"Cannot stat animation package");
    if (!S_ISREG(before.st_mode) || before.st_size<44 || static_cast<std::uint64_t>(before.st_size)>max_clip_bytes)
        fail("Animation package must be a bounded regular file");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(before.st_size));
    std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=read(fd,bytes.data()+offset,bytes.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) fail("Cannot read complete animation package");
        offset+=static_cast<std::size_t>(n);
    }
    std::uint8_t extra;
    ssize_t tail;
    do { tail=read(fd,&extra,1); } while (tail<0 && errno==EINTR);
    if (tail!=0 || fstat(fd,&after) || changed(before,after))
        fail("Animation package changed during read");
    return decode_clip(bytes);
}

std::vector<LocalPose> Clip::pose_at(double seconds) const {
    if (!std::isfinite(seconds)) fail("Invalid animation time");
    const double last=static_cast<double>(samples.size()-1);
    const double frame=std::clamp(seconds*fps,0.0,last);
    const auto left=static_cast<std::size_t>(frame);
    const auto right=std::min(left+1,samples.size()-1);
    const double amount=frame-left;
    std::vector<LocalPose> pose(samples[left].size());
    for (std::size_t bone=0;bone<pose.size();++bone) {
        const auto& a=samples[left][bone];
        const auto& b=samples[right][bone];
        pose[bone].rotation=interpolate(a.rotation,b.rotation,amount);
        if (a.translation || b.translation) {
            const auto& rest=skeleton.rest_local_translation(bone);
            const auto& ta=a.translation ? *a.translation : rest;
            const auto& tb=b.translation ? *b.translation : rest;
            pose[bone].translation=Vec3{
                ta[0]+(tb[0]-ta[0])*amount,
                ta[1]+(tb[1]-ta[1])*amount,
                ta[2]+(tb[2]-ta[2])*amount};
        }
    }
    return pose;
}

std::vector<Vec3> Clip::positions_at(double seconds) const {
    return skin_positions(vertices,skeleton.evaluate(pose_at(seconds)));
}

std::vector<Vec3> Clip::positions(std::size_t sample) const {
    if (sample>=samples.size()) fail("Animation sample out of range");
    return skin_positions(vertices,skeleton.evaluate(samples[sample]));
}

assets::Mesh Clip::mesh_at(double seconds) const { return make_mesh(positions_at(seconds),indices,centre,radius); }
assets::Mesh Clip::mesh(std::size_t sample) const { return make_mesh(positions(sample),indices,centre,radius); }

Playback::Playback(std::size_t samples,std::uint32_t fps):samples_(samples),fps_(fps) {
    if (!samples || samples>512 || !fps || fps>240) fail("Invalid playback counts");
}
void Playback::advance(std::uint64_t ns) {
    // Round up the last sample's timestamp so fractional nanoseconds cannot
    // select the endpoint early. Saturation also prevents overflow after stalls.
    const std::uint64_t end=((samples_-1)*1'000'000'000ULL+fps_-1)/fps_;
    elapsed_+=std::min(ns,end-elapsed_);
}
std::size_t Playback::sample() const {
    return std::min(samples_-1,static_cast<std::size_t>(elapsed_*fps_/1'000'000'000ULL));
}
}
