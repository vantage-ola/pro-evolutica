#include "../assets/mesh.h"
#include "../rendering/cube_data.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
const std::string fixture =
    "# Original four-vertex plane\no original\n"
    "v 10 20 30\nv 12 20 30\nv 10 23 30\nv 12 23 30\n"
    "vt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\n"
    "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
    "f 1/1/1 2/2/2 3/3/3\nf 3/3/3 2/2/2 4/4/4\n";

void reject(const std::string& text) {
    bool rejected = false;
    try { assets::decode_obj(text); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "Bad OBJ accepted");
}
std::string replace(std::string text, const std::string& from, const std::string& to) {
    const auto p = text.find(from);
    if (p == std::string::npos) throw std::runtime_error("Invalid test replacement");
    text.replace(p, from.size(), to);
    return text;
}

void geometry() {
    const auto mesh = assets::decode_obj(fixture);
    check(mesh.vertices.size() == 4, "Wrong vertex count");
    check(mesh.indices == std::vector<std::uint16_t>({0,1,2,2,1,3}), "Winding/order changed");
    check(mesh.centre == std::array<float,3>({11,21.5f,30}), "Wrong centre");
    check(std::abs(mesh.radius - std::sqrt(3.25f)) < 1e-6, "Wrong radius");
    check(mesh.vertices[3].x == 12 && mesh.vertices[3].y == 23 && mesh.vertices[3].z == 30,
          "Reader changed source positions");
    check(mesh.vertices[3].u == 1 && mesh.vertices[3].v == 1 && mesh.vertices[3].nz == 1,
          "Reader changed UVs or normals");
    auto windows = fixture;
    for (std::size_t p = 0; (p = windows.find('\n',p)) != std::string::npos; p += 2)
        windows.insert(p, "\r");
    check(assets::decode_obj(windows).indices == mesh.indices, "CRLF handling changed faces");
    check(assets::decode_obj(replace(fixture,"v 10 20 30","v 1e1 2e1 3e1 # note")).centre == mesh.centre,
          "Comments/exponents are not parsed correctly");
    check(assets::decode_obj(fixture.substr(0,fixture.size()-1)).indices == mesh.indices,
          "Missing final newline changed faces");
    // Normalise framing only, then rotate/project at portrait and landscape sizes.
    for (float aspect : {.25f,.5f,1.f,2.f,4.f})
        for (int step = 0; step < 72; ++step) {
            const auto matrix = cube_data::transform(step*.0872664626f,aspect);
            for (const auto& v : mesh.vertices) {
                const float p[] = {(v.x-mesh.centre[0])/mesh.radius,
                    (v.y-mesh.centre[1])/mesh.radius,(v.z-mesh.centre[2])/mesh.radius,1};
                float clip[4]{};
                for (int row = 0; row < 4; ++row)
                    for (int col = 0; col < 4; ++col) clip[row] += matrix[col*4+row]*p[col];
                bool visible = clip[3] > 0;
                for (int i = 0; i < 3; ++i) visible &= std::isfinite(clip[i]) && std::abs(clip[i]/clip[3]) < 1;
                check(visible,"Framed mesh was clipped during rotation");
            }
        }
}

void malformed() {
    for (const auto& text : {std::string(), std::string("v 0 0 0\n"), std::string("\0",1)}) reject(text);
    for (const auto& tail : {"mtllib file.mtl\n", "usemtl test\n", "g group\n", "s 1\n", "o second\n"})
        reject(fixture + tail);
    for (const auto& value : {"nan", "inf", "-inf", "1e100", "1000001", "x", "10junk"})
        reject(replace(fixture, "v 10 20 30", std::string("v ")+value+" 20 30"));
    for (const auto& face : {"f 1/1/1 2/2/2", "f 1/1/1 2/2/2 3/3/3 4/4/4", "f 0/0/0 2/2/2 3/3/3",
        "f -1/-1/-1 2/2/2 3/3/3", "f 5/5/5 2/2/2 3/3/3", "f 1//1 2/2/2 3/3/3",
        "f 1/1 2/2/2 3/3/3", "f 1/2/1 2/2/2 3/3/3", "f 1/1/1/1 2/2/2 3/3/3",
        "f 65536/65536/65536 2/2/2 3/3/3", "f 99999999999999/1/1 2/2/2 3/3/3",
        "f 1/1/1 1/1/1 3/3/3"})
        reject(replace(fixture,"f 1/1/1 2/2/2 3/3/3",face));
    reject(replace(fixture,"v 10 23 30","v 11 20 30"));
    reject(replace(fixture,"vn 0 0 1","vn 0 0 0"));
    reject(replace(fixture,"vt 0 0\n",""));
    reject(replace(fixture,"vn 0 0 1\n",""));
    reject(replace(fixture,"vt 0 0","vt 0 0 1"));
    reject(replace(fixture,"v 10 20 30","v 10 20"));
    reject(fixture + std::string(513,'#'));
    reject(std::string(assets::max_obj_bytes + 1,'#'));
    reject(std::string(500'001,'\n'));
    for (const auto& record : {"v 0 0 0\n", "vt 0 0\n", "vn 0 0 1\n"}) {
        std::string input;
        for (std::size_t i = 0; i <= assets::max_mesh_vertices; ++i) input += record;
        reject(input);
    }
    std::string input;
    for (std::size_t i = 0; i <= assets::max_mesh_indices/3; ++i) input += "f 1/1/1 2/2/2 3/3/3\n";
    reject(input);
}

void files() {
    char pattern[] = "/tmp/pro-evolutica-mesh-XXXXXX";
    const char* directory = mkdtemp(pattern);
    if (!directory) throw std::runtime_error("Cannot create temporary directory");
    const std::filesystem::path root(directory);
    struct Cleanup { std::filesystem::path p; ~Cleanup() { std::filesystem::remove_all(p); } } cleanup{root};
    const auto source = root/"mesh.obj";
    { std::ofstream out(source); out << fixture; }
    check(assets::load_obj(source.string()).indices.size() == 6, "Regular file failed");
    std::filesystem::create_symlink(source,root/"link");
    check(mkfifo((root/"fifo").c_str(),0600) == 0,"Cannot create FIFO");
    { std::ofstream out(root/"large"); out.seekp(assets::max_obj_bytes); out.put('x'); }
    for (const auto& p : {root/"missing", root/"link", root/"fifo", root/"large", root}) {
        bool rejected = false;
        try { assets::load_obj(p.string()); } catch (const std::exception&) { rejected = true; }
        check(rejected,"Unsafe or oversized file accepted");
    }
}
} // namespace

int main() {
    try {
        geometry(); malformed(); files();
        std::cout << checks << " mesh checks passed: geometry, framing, format, bounds, files\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
