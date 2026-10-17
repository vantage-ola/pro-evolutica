#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace animation {
using Vec3 = std::array<double, 3>;
using Quaternion = std::array<double, 4>; // XYZW
using Matrix = std::array<double, 16>; // Row vectors; translation at 12..14.
constexpr std::size_t max_bones = 128;
constexpr std::size_t max_skin_vertices = 65'535;

struct Bone {
    std::int32_t parent;
    Matrix rest_global;
    Matrix inverse_bind;
};
struct LocalPose {
    Quaternion rotation{0, 0, 0, 1};
    std::optional<Vec3> translation; // Missing: retain local rest translation.
};
struct Pose {
    std::vector<Matrix> global;
    std::vector<Matrix> skin;
};
struct SkinVertex {
    Vec3 position;
    std::array<std::uint8_t, 4> bones{}; // Global bone indices, already palette-mapped.
    std::array<double, 4> weights{}; // Residual slot resolved by the caller.
};

// Observed dense ANM packing only; rejects the unknown reserved bit.
Quaternion decode_rotation(const std::array<std::uint8_t, 6>& packed);

// Validate rigid bind matrices and hierarchy once, before evaluating any poses.
class Skeleton {
public:
    explicit Skeleton(std::vector<Bone> bones);
    Pose evaluate(const std::vector<LocalPose>& local) const;
    const Vec3& rest_local_translation(std::size_t bone) const;
private:
    std::vector<Bone> bones_;
    std::vector<Vec3> rest_translation_;
    std::vector<std::size_t> order_;
};

// Position deformation only; no normals, materials, timing or file loading.
std::vector<Vec3> skin_positions(const std::vector<SkinVertex>& vertices, const Pose& pose);
} // namespace animation
