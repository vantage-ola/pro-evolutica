#include "../animation/skeleton.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace animation;
const Matrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void near(const Vec3& actual, const Vec3& expected, double tolerance = 1e-10) {
    double squared = 0;
    for (int i = 0; i < 3; ++i) squared += (actual[i]-expected[i])*(actual[i]-expected[i]);
    check(std::sqrt(squared) < tolerance, "Known original pose differs");
}
template<class F> void reject(F operation) {
    bool failed = false;
    try { operation(); } catch (const std::runtime_error&) { failed = true; }
    check(failed, "Invalid skeletal input accepted");
}
std::vector<Bone> bones() {
    auto rest = identity, inverse = identity;
    rest[12] = 2;
    inverse[12] = -2;
    return {{-1,identity,identity},{0,rest,inverse}};
}
std::vector<LocalPose> poses() {
    return {LocalPose{},LocalPose{}};
}
SkinVertex vertex() { return {{3,0,0},{0,1,0,0},{.75,.25,0,0}}; }
Quaternion z90() { const double s = std::sqrt(.5); return {0,0,s,s}; }

void joint_and_weights() {
    Skeleton skeleton(bones());
    auto local = poses();
    near(skin_positions({vertex()},skeleton.evaluate(local))[0],{3,0,0});
    local[1].rotation = z90();
    auto pose = skeleton.evaluate(local);
    near(skin_positions({vertex()},pose)[0],{2.75,.25,0});
    auto v = vertex();
    v.weights = {.25,.75,0,0};
    near(skin_positions({v},pose)[0],{2.25,.75,0});
    near({pose.global[1][12],pose.global[1][13],pose.global[1][14]},{2,0,0});
}

void hierarchy_and_rest_rotation() {
    auto b = bones();
    b[0].parent = 1; b[0].rest_global = b[1].rest_global; b[0].inverse_bind = b[1].inverse_bind;
    b[1] = {-1,identity,identity}; // Child precedes root in storage.
    auto local = poses();
    local[1].rotation = z90(); local[1].translation = Vec3{10,20,30};
    auto pose = Skeleton(b).evaluate(local);
    near({pose.global[0][12],pose.global[0][13],pose.global[0][14]},{10,22,30});
    near(skin_positions({{{3,0,0},{0,0,0,0},{1,0,0,0}}},pose)[0],{10,23,30});
    // A matching animated local rotation replaces the rotated rest orientation.
    b = bones();
    b[1].rest_global = {0,1,0,0, -1,0,0,0, 0,0,1,0, 2,0,0,1};
    b[1].inverse_bind = {0,-1,0,0, 1,0,0,0, 0,0,1,0, 0,2,0,1};
    local = poses(); local[1].rotation = z90();
    near(skin_positions({vertex()},Skeleton(b).evaluate(local))[0],{3,0,0});
}

void packing() {
    const auto q = decode_rotation({3,0,1,128,0,64});
    check(std::abs(q[0]-.5/23169.767578125) < 1e-15 && q[3] > .999999,
          "Literal identity packing differs");
    auto local = poses(); local[1].rotation = decode_rotation({255,255,1,128,0,64});
    near(skin_positions({vertex()},Skeleton(bones()).evaluate(local))[0],{2.75,.25,0},1e-4);
    reject([] { decode_rotation({0,0,0,0,0,128}); });
    reject([] { decode_rotation({252,255,255,255,255,127}); });
}

void invalid_inputs() {
    reject([] { Skeleton s({}); });
    auto b = bones(); b[0].parent = 1;
    reject([&] { Skeleton s(b); }); // Cycle.
    b = bones(); b[1].parent = 2;
    reject([&] { Skeleton s(b); });
    b = bones(); b[1].inverse_bind[12] = 0;
    reject([&] { Skeleton s(b); });
    b = bones(); b[0].rest_global[0] = 2;
    reject([&] { Skeleton s(b); });
    b = bones(); b[0].rest_global[0] = b[0].inverse_bind[0] = -1;
    reject([&] { Skeleton s(b); });
    Skeleton skeleton(bones());
    reject([&] { skeleton.evaluate({LocalPose{}}); });
    auto local = poses(); local[0].rotation = {0,0,0,0};
    reject([&] { skeleton.evaluate(local); });
    local = poses(); local[0].translation = Vec3{0,std::numeric_limits<double>::infinity(),0};
    reject([&] { skeleton.evaluate(local); });
    auto pose = skeleton.evaluate(poses());
    auto v = vertex(); v.weights[0] = -.75;
    reject([&] { skin_positions({v},pose); });
    v = vertex(); v.weights[0] = 0;
    reject([&] { skin_positions({v},pose); });
    v = vertex(); v.bones[1] = 2;
    reject([&] { skin_positions({v},pose); });
    reject([&] { skin_positions(std::vector<SkinVertex>(max_skin_vertices+1,vertex()),pose); });
}
} // namespace

int main() {
    try {
        joint_and_weights();
        hierarchy_and_rest_rotation();
        packing();
        invalid_inputs();
        std::cout << "4 focused skeleton check groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
