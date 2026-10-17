#include "../rendering/camera.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool close(float a, float b) { return std::abs(a-b) < 1e-5f; }
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected,"Invalid camera input accepted");
}
std::array<float,4> project(const cube_data::Matrix& m, float x, float y, float z) {
    const float p[] = {x,y,z,1};
    std::array<float,4> result{};
    for (int row=0; row<4; ++row)
        for (int col=0; col<4; ++col) result[row] += m[col*4+row]*p[col];
    return result;
}
void framing() {
    camera::View view;
    // Default orbit retains the old framing. Far-plane distance intentionally differs.
    const auto old = cube_data::transform(.6f,2);
    const auto now = view.transform(.6f,2,1);
    for (int col=0; col<4; ++col) for (int row : {0,1,3})
        check(close(old[col*4+row],now[col*4+row]),"Default camera changes framing");
    view.orbit(1.57079632679f,0);
    const auto right = project(view.transform(0,1,1),1,0,0);
    check(close(right[0],0) && close(right[3],3.5f),"Orbit handedness/order changed");
    view.reset(); view.orbit(0,1);
    const auto top = project(view.transform(0,1,1),0,1,0);
    check(top[1] > 0 && top[3] > 4.5f,"Pitch does not orbit around centre");
    view.reset();
    const auto portrait = project(view.transform(0,.5f,1),1,0,0);
    check(portrait[0]/portrait[3] > .65f && portrait[0]/portrait[3] < 1.f,
          "Portrait camera should fill more of the narrow screen without clipping");
    for (float aspect : {.1f,.25f,.5f,1.f,2.f,4.f,10.f})
        for (float zoom : {.5f,1.f,3.f})
            for (int step=0; step<36; ++step)
                for (float pitch : {-1.45f,0.f,1.45f}) {
                    view = {step*.1745329252f,pitch,zoom};
                    const auto matrix = view.transform(step*.13f,aspect,std::sqrt(3.f));
                    for (const auto& v : cube_data::vertices) {
                        const auto p = project(matrix,v.x,v.y,v.z);
                        bool inside = p[3] > 0;
                        for (int axis=0; axis<3; ++axis)
                            inside &= std::isfinite(p[axis]) && std::abs(p[axis]/p[3]) < 1;
                        check(inside,"Cube clipped at supported camera bounds");
                    }
                    const auto ball = view.transform(step*.13f,aspect,1);
                    // Sample a sphere rather than a cube to exercise the mesh's radius.
                    for (int i=0; i<24; ++i) {
                        const float a = i*.2617993878f;
                        const auto p = project(ball,std::cos(a),std::sin(a),0);
                        bool inside = p[3] > 0;
                        for (int axis=0; axis<3; ++axis)
                            inside &= std::isfinite(p[axis]) && std::abs(p[axis]/p[3]) < 1;
                        check(inside,"Mesh clipped at supported camera bounds");
                    }
                }
}
void bounds_and_sticks() {
    camera::View view;
    view.orbit(1e30f,1e30f); view.dolly(-1e30f);
    check(std::abs(view.yaw) <= 3.142f && close(view.pitch,1.45f) && view.zoom == .5f,"Camera limits failed");
    view.orbit(-1e30f,-1e30f); view.dolly(1e30f);
    check(close(view.pitch,-1.45f) && view.zoom == 3,"Opposite camera limits failed");
    view.reset(); check(view.yaw == 0 && view.pitch == 0 && view.zoom == 1,"Reset failed");
    check(camera::stick(0) == 0 && camera::stick(5000) == 0 && camera::stick(-5000) == 0,"Stick drift not filtered");
    check(camera::stick(-32768) == -1 && camera::stick(32767) == 1,"Stick endpoints changed");
    for (int value=-32768; value<=32767; value+=257)
        check(std::abs(camera::stick(static_cast<std::int16_t>(value))) <= 1,"Stick outside unit range");
    camera::gamepad(view,0,0,0,.1f);
    check(view.yaw == 0 && view.pitch == 0 && view.zoom == 1,"Neutral stick changes camera");
    auto slow = view;
    camera::gamepad(view,20000,-15000,18000,.1f);
    for (int i=0;i<10;++i) camera::gamepad(slow,20000,-15000,18000,.01f);
    check(close(view.yaw,slow.yaw) && close(view.pitch,slow.pitch) && close(view.zoom,slow.zoom),"Stick speed depends on FPS");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float bad : {nan,inf,-inf}) {
        rejects([&]{view.orbit(bad,0);}); rejects([&]{view.orbit(0,bad);});
        rejects([&]{view.dolly(bad);}); rejects([&]{view.transform(bad,1,1);});
        rejects([&]{view.transform(0,bad,1);}); rejects([&]{view.transform(0,1,bad);});
        rejects([&]{camera::gamepad(view,0,0,0,bad);});
    }
    for (float aspect : {0.f,-1.f,.09f,11.f}) rejects([&]{view.transform(0,aspect,1);});
    check(std::isfinite(view.transform(0, 1, 3)[0]), "Match-sized camera radius rejected");
    for (float radius : {0.f,-1.f,11.f}) rejects([&]{view.transform(0,1,radius);});
    rejects([&]{camera::gamepad(view,0,0,0,-.1f);});
    rejects([&]{camera::gamepad(view,0,0,0,.2f);});
}
void touch() {
    camera::View view;
    camera::Touch touch;
    touch.down(7,100,100,0); touch.move(7,102,100,500,view);
    check(touch.up(7,.2) == camera::Action::tap && view.yaw == 0,"Tap/jitter changes camera");
    check(touch.up(7,.3) == camera::Action::none,"Duplicate release creates tap");
    touch.down(7,100,100,1);
    check(touch.up(7,1.7) == camera::Action::reset,"Hold did not reset");
    touch.down(7,100,100,2); touch.move(7,200,150,500,view);
    check(view.yaw > 0 && view.pitch > 0 && touch.up(7,2.3) == camera::Action::orbit,"Drag became a tint tap");
    touch.down(7,100,100,3); touch.down(8,200,100,3);
    touch.move(8,300,100,500,view);
    check(close(view.zoom,.5f),"Pinch does not use distance ratio");
    check(touch.up(8,3.2) == camera::Action::none,"First pinch release became tap");
    const auto yaw = view.yaw;
    touch.move(7,120,100,500,view);
    check(view.yaw == yaw && touch.up(7,3.3) == camera::Action::zoom,"Remaining pinch finger jumps camera");
    touch.down(1,0,0,4); touch.down(2,0,0,4); touch.down(3,0,0,4);
    touch.move(2,1,0,500,view);
    check(std::isfinite(view.zoom),"Coincident pinch produced invalid zoom");
    check(touch.up(3,4.1) == camera::Action::none,"Untracked finger became tap");
    touch.up(1,4.1); check(touch.up(2,4.1) == camera::Action::zoom,"Extra finger causes tap");
    touch.down(5,10,10,5); touch.cancel();
    check(touch.up(5,6) == camera::Action::none,"Cancelled gesture became reset");
    touch.down(5,10,10,6); touch.down(5,10,10,6.1);
    check(touch.up(5,6.2) == camera::Action::tap,"Duplicate down creates phantom finger");
    touch.down(6,10,10,7); touch.move(99,30,30,500,view);
    check(touch.up(6,7.2) == camera::Action::tap,"Untracked motion affects gesture");
    rejects([&]{touch.move(1,0,0,0,view);});
    rejects([&]{touch.down(1,0,0,std::numeric_limits<double>::infinity());});
}
}
int main() {
    try {
        framing(); bounds_and_sticks(); touch();
        std::cout << checks << " camera checks passed: framing, bounds, sticks, touch\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
