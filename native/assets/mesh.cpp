#include "mesh.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace assets {
namespace {
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

void end_of_line(std::istringstream& line) {
    std::string extra;
    if (line >> extra) fail("Unsupported trailing OBJ values");
}

template<std::size_t N> std::array<float, N> values(std::istringstream& line) {
    std::array<float, N> out{};
    for (auto& x : out)
        if (!(line >> x) || !std::isfinite(x) || std::abs(x) > 1'000'000)
            fail("Invalid or excessive OBJ attribute");
    end_of_line(line);
    return out;
}

std::uint16_t face_index(std::string_view token) {
    std::uint32_t indices[3]{};
    for (int i = 0; i < 3; ++i) {
        const auto slash = token.find('/');
        const auto part = token.substr(0, slash);
        if (part.empty()) fail("OBJ face needs positive v/vt/vn indices");
        const auto result = std::from_chars(part.data(), part.data() + part.size(), indices[i]);
        if (result.ec != std::errc{} || result.ptr != part.data() + part.size() ||
            !indices[i] || indices[i] > max_mesh_vertices)
            fail("Invalid OBJ face index");
        if ((i < 2) != (slash != std::string_view::npos))
            fail("OBJ face needs exactly three indices per corner");
        if (slash != std::string_view::npos) token.remove_prefix(slash + 1);
    }
    if (indices[0] != indices[1] || indices[0] != indices[2])
        fail("Only matching OBJ position/UV/normal indices are supported");
    return static_cast<std::uint16_t>(indices[0] - 1);
}

bool changed(const struct stat& a, const struct stat& b) {
#ifdef __APPLE__
    return a.st_size != b.st_size || a.st_mtimespec.tv_sec != b.st_mtimespec.tv_sec ||
        a.st_mtimespec.tv_nsec != b.st_mtimespec.tv_nsec || a.st_ctimespec.tv_sec != b.st_ctimespec.tv_sec ||
        a.st_ctimespec.tv_nsec != b.st_ctimespec.tv_nsec;
#else
    return a.st_size != b.st_size || a.st_mtim.tv_sec != b.st_mtim.tv_sec ||
        a.st_mtim.tv_nsec != b.st_mtim.tv_nsec || a.st_ctim.tv_sec != b.st_ctim.tv_sec ||
        a.st_ctim.tv_nsec != b.st_ctim.tv_nsec;
#endif
}
} // namespace

Mesh decode_obj(std::string_view text) {
    if (text.size() > max_obj_bytes) fail("OBJ exceeds 32 MiB input limit");
    std::vector<std::array<float, 3>> positions, normals;
    std::vector<std::array<float, 2>> uvs;
    Mesh mesh;
    std::size_t lines = 0, objects = 0;
    while (!text.empty()) {
        if (++lines > 500'000) fail("OBJ exceeds line count limit");
        const auto newline = text.find('\n');
        auto raw = text.substr(0, newline);
        text.remove_prefix(newline == std::string_view::npos ? text.size() : newline + 1);
        if (raw.size() > 512) fail("OBJ exceeds line length limit");
        if (raw.find('\0') != std::string_view::npos) fail("OBJ contains a NUL byte");
        raw = raw.substr(0, raw.find('#'));
        std::istringstream line{std::string(raw)};
        line.imbue(std::locale::classic());
        std::string kind;
        if (!(line >> kind)) continue;
        if (kind == "v") {
            if (positions.size() == max_mesh_vertices) fail("OBJ exceeds vertex limit");
            positions.push_back(values<3>(line));
        } else if (kind == "vt") {
            if (uvs.size() == max_mesh_vertices) fail("OBJ exceeds UV limit");
            uvs.push_back(values<2>(line));
        } else if (kind == "vn") {
            if (normals.size() == max_mesh_vertices) fail("OBJ exceeds normal limit");
            auto normal = values<3>(line);
            const double length = std::hypot(double(normal[0]), double(normal[1]), double(normal[2]));
            if (length < 1e-6) fail("OBJ has a zero or negligible normal");
            normals.push_back(normal);
        } else if (kind == "f") {
            if (mesh.indices.size() > max_mesh_indices - 3) fail("OBJ exceeds index limit");
            for (int i = 0; i < 3; ++i) {
                std::string token;
                if (!(line >> token)) fail("OBJ requires triangle faces");
                mesh.indices.push_back(face_index(token));
            }
            end_of_line(line);
        } else if (kind == "o") {
            std::string name;
            if (++objects > 1 || !(line >> name)) fail("OBJ supports one named object");
            end_of_line(line);
        } else fail("Unsupported OBJ record; no external materials are loaded");
    }
    if (positions.empty() || positions.size() != normals.size() || positions.size() != uvs.size() ||
        mesh.indices.empty()) fail("OBJ requires matching position/UV/normal arrays and faces");
    auto lower = positions[0], upper = lower;
    mesh.vertices.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i]; const auto& n = normals[i]; const auto& uv = uvs[i];
        mesh.vertices.push_back({p[0],p[1],p[2],uv[0],uv[1],n[0],n[1],n[2]});
        for (int axis = 0; axis < 3; ++axis) {
            lower[axis] = std::min(lower[axis], p[axis]);
            upper[axis] = std::max(upper[axis], p[axis]);
        }
    }
    for (int axis = 0; axis < 3; ++axis) mesh.centre[axis] = (lower[axis] + upper[axis]) * .5f;
    double radius = 0;
    for (const auto& p : positions)
        radius = std::max(radius, std::hypot(double(p[0]) - mesh.centre[0],
            double(p[1]) - mesh.centre[1], double(p[2]) - mesh.centre[2]));
    if (radius < 1e-6) fail("OBJ has negligible spatial bounds");
    mesh.radius = static_cast<float>(radius);
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto a = mesh.indices[i], b = mesh.indices[i+1], c = mesh.indices[i+2];
        if (a >= positions.size() || b >= positions.size() || c >= positions.size())
            fail("OBJ index exceeds vertex count");
        if (a == b || a == c || b == c) fail("OBJ contains repeated triangle indices");
        double e[3], f[3];
        for (int axis = 0; axis < 3; ++axis) {
            e[axis] = double(positions[b][axis]) - positions[a][axis];
            f[axis] = double(positions[c][axis]) - positions[a][axis];
        }
        if (std::hypot(e[1]*f[2]-e[2]*f[1], e[2]*f[0]-e[0]*f[2], e[0]*f[1]-e[1]*f[0]) == 0)
            fail("OBJ contains a zero-area triangle");
    }
    return mesh;
}

Mesh load_obj(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "Open OBJ");
    struct Close { int fd; ~Close() { close(fd); } } close_file{fd};
    struct stat before{}, after{};
    if (fstat(fd, &before)) throw std::system_error(errno, std::generic_category(), "Stat OBJ");
    if (!S_ISREG(before.st_mode)) fail("OBJ input must be a regular file");
    if (before.st_size < 0 || std::uint64_t(before.st_size) > max_obj_bytes)
        fail("OBJ exceeds 32 MiB input limit");
    std::string bytes(static_cast<std::size_t>(before.st_size), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::system_error(errno, std::generic_category(), "Read OBJ");
        if (!count) fail("OBJ was truncated while reading");
        offset += static_cast<std::size_t>(count);
    }
    char extra;
    ssize_t count;
    do { count = read(fd, &extra, 1); } while (count < 0 && errno == EINTR);
    if (count != 0 || fstat(fd, &after) || changed(before, after)) fail("OBJ changed or failed while reading");
    return decode_obj(bytes);
}
} // namespace assets
