#include "skeleton.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace animation {
namespace {
constexpr double tolerance = 2e-5;
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

template<std::size_t N> bool finite(const std::array<double, N>& values) {
    for (double x : values) if (!std::isfinite(x)) return false;
    return true;
}
Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix out{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k) out[r*4+c] += a[r*4+k]*b[k*4+c];
    return out;
}
void rigid(const Matrix& m) {
    if (!finite(m)) fail("Non-finite bind matrix");
    for (int k : {3, 7, 11, 15})
        if (std::abs(m[k] - (k == 15)) > tolerance) fail("Bind matrix is not affine");
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            double dot = 0;
            for (int k = 0; k < 3; ++k) dot += m[r*4+k]*m[c*4+k];
            if (std::abs(dot - (r == c)) > tolerance) fail("Bind matrix is not rigid");
        }
    const double determinant = m[0]*(m[5]*m[10]-m[6]*m[9]) -
        m[1]*(m[4]*m[10]-m[6]*m[8]) + m[2]*(m[4]*m[9]-m[5]*m[8]);
    if (std::abs(determinant - 1) > tolerance) fail("Reflected bind matrix");
}
Matrix rotation(Quaternion q) {
    if (!finite(q)) fail("Non-finite local rotation");
    double length2 = 0;
    for (double x : q) length2 += x*x;
    if (std::abs(length2 - 1) > tolerance) fail("Local rotation is not a unit quaternion");
    const double length = std::sqrt(length2);
    for (double& x : q) x /= length;
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    return {1-2*(y*y+z*z), 2*(x*y+z*w), 2*(x*z-y*w), 0,
            2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w), 0,
            2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x*x+y*y), 0,
            0, 0, 0, 1};
}
} // namespace

Quaternion decode_rotation(const std::array<std::uint8_t, 6>& packed) {
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < packed.size(); ++i) bits |= std::uint64_t(packed[i]) << (i*8);
    if (bits >> 47) fail("Unsupported packed rotation reserved bit");
    const auto missing = static_cast<std::size_t>(bits & 3);
    std::array<double, 3> stored{};
    double sum = 0;
    for (int i = 0; i < 3; ++i) {
        stored[i] = (double((bits >> (32-i*15)) & 32767) - 16383.5) / 23169.767578125;
        sum += stored[i]*stored[i];
    }
    if (sum > 1) fail("Packed rotation exceeds unit length");
    Quaternion q{};
    for (std::size_t i = 0, j = 0; i < q.size(); ++i)
        q[i] = i == missing ? std::sqrt(1-sum) : stored[j++];
    double length2 = 0;
    for (double x : q) length2 += x*x;
    for (double& x : q) x /= std::sqrt(length2);
    return q;
}

Skeleton::Skeleton(std::vector<Bone> bones) : bones_(std::move(bones)) {
    if (bones_.empty() || bones_.size() > max_bones) fail("Unsupported bone count");
    rest_translation_.resize(bones_.size());
    for (const auto& b : bones_) {
        if (b.parent < -1 || b.parent >= static_cast<std::int32_t>(bones_.size())) fail("Invalid bone parent");
        rigid(b.rest_global);
        rigid(b.inverse_bind);
        const auto inverse = multiply(b.rest_global, b.inverse_bind);
        if (!finite(inverse)) fail("Non-finite bind inverse product");
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (std::abs(inverse[r*4+c] - (r == c)) > tolerance) fail("Bind inverse mismatch");
    }
    std::vector<unsigned char> state(bones_.size());
    const auto visit = [&](auto&& self, std::size_t node) -> void {
        if (state[node] == 1) fail("Cyclic bone hierarchy");
        if (state[node] == 2) return;
        state[node] = 1;
        const auto parent = bones_[node].parent;
        if (parent != -1) self(self, static_cast<std::size_t>(parent));
        state[node] = 2;
        order_.push_back(node);
    };
    for (std::size_t i = 0; i < bones_.size(); ++i) {
        visit(visit, i);
        const auto& bone = bones_[i];
        const auto local = bone.parent == -1 ? bone.rest_global :
            multiply(bone.rest_global, bones_[bone.parent].inverse_bind);
        rest_translation_[i] = {local[12], local[13], local[14]};
    }
}

Pose Skeleton::evaluate(const std::vector<LocalPose>& local) const {
    if (local.size() != bones_.size()) fail("Pose must contain every bone");
    Pose out;
    out.global.resize(bones_.size());
    out.skin.resize(bones_.size());
    for (std::size_t node : order_) {
        Matrix matrix = rotation(local[node].rotation);
        const auto translation = local[node].translation.value_or(rest_translation_[node]);
        if (!finite(translation)) fail("Non-finite local translation");
        for (int c = 0; c < 3; ++c) matrix[12+c] = translation[c];
        const auto parent = bones_[node].parent;
        out.global[node] = parent == -1 ? matrix : multiply(matrix, out.global[parent]);
        out.skin[node] = multiply(bones_[node].inverse_bind, out.global[node]);
        if (!finite(out.global[node]) || !finite(out.skin[node])) fail("Non-finite evaluated transform");
    }
    return out;
}

std::vector<Vec3> skin_positions(const std::vector<SkinVertex>& vertices, const Pose& pose) {
    if (vertices.empty() || vertices.size() > max_skin_vertices) fail("Unsupported skin vertex count");
    if (pose.skin.empty() || pose.skin.size() > max_bones) fail("Unsupported skin transform count");
    for (const auto& m : pose.skin) if (!finite(m)) fail("Non-finite skin transform");
    std::vector<Vec3> out;
    out.reserve(vertices.size());
    for (const auto& v : vertices) {
        if (!finite(v.position) || !finite(v.weights)) fail("Non-finite skin vertex");
        double total = 0;
        for (double weight : v.weights) {
            if (weight < 0 || weight > 1) fail("Invalid skin weight");
            total += weight;
        }
        if (std::abs(total - 1) > 2e-6) fail("Skin weights must sum to one");
        Vec3 moved{};
        for (std::size_t i = 0; i < v.weights.size(); ++i) {
            if (v.weights[i] == 0) continue;
            if (v.bones[i] >= pose.skin.size()) fail("Skin bone index is out of range");
            const auto& m = pose.skin[v.bones[i]];
            for (int c = 0; c < 3; ++c) {
                double value = m[12+c];
                for (int k = 0; k < 3; ++k) value += v.position[k]*m[k*4+c];
                moved[c] += (v.weights[i]/total)*value;
            }
        }
        if (!finite(moved)) fail("Non-finite deformed vertex");
        out.push_back(moved);
    }
    return out;
}
} // namespace animation
