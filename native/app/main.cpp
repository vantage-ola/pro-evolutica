#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../rendering/cube.h"
#include "../diagnostics/frame_stats.h"
#include "../animation/clip.h"
#include "../assets/resource.h"
#include "../assets/texture.h"
#include "../assets/bundle.h"
#include "../assets/ktmdl.h"
#include <memory>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <system_error>
#include <stdexcept>
#include <zlib.h>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>

namespace {
struct App {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_Gamepad* gamepad = nullptr;
    Cube cube;
    std::vector<skinning::Instance> skinned_instances;
    std::unique_ptr<animation::Clip> clip;
    std::unique_ptr<animation::Playback> playback;
    std::size_t uploaded_sample = 0;
    std::uint64_t uploaded_elapsed_ns = 0;
    camera::View view;
    camera::Touch touch;
    diagnostics::Capture capture;
    int capture_width = 0, capture_height = 0;
    std::size_t mesh_vertices = 0, mesh_triangles = 0;
    unsigned long mesh_vertex_crc = 0, mesh_index_crc = 0, texture_crc = 0;
    Uint64 main_begin = 0;
    double mesh_read_ms = 0, mesh_upload_ms = 0, image_read_ms = 0, image_upload_ms = 0;
    double create_ms = 0, first_frame_ms = 0;
    bool gamepad_moving = false;
    int colour = 0;
    bool vsync = false;
    bool mesh_loaded = false;
    bool archive_ball = false;
    bool paused = false;
    float angle = .6f;
    // Android can deliver lifecycle events from a different thread.
    std::atomic<bool> clear_touch{false};
    std::atomic<bool> active{true};
    std::atomic<bool> running{true};
};

bool SDLCALL lifecycle(void* userdata, SDL_Event* event) {
    auto& app = *static_cast<App*>(userdata);
    switch (event->type) {
    case SDL_EVENT_TERMINATING:
        app.running = false;
        return false;
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        app.clear_touch = true;
        app.active = false;
        return false;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        app.clear_touch = true;
        app.active = true;
        return false;
    default:
        return true;
    }
}

void change_colour(App& app, const char* input) {
    app.colour = (app.colour + 1) % 3;
    SDL_Log("Input: %s; colour=%d", input, app.colour);
}

void pause_rotation(App& app) {
    if (app.playback && app.playback->ended()) {
        app.playback->rewind();
        app.paused = false;
    } else app.paused = !app.paused;
    if (app.playback)
        SDL_Log("Animation %s; sample=%zu",app.paused ? "paused" : "playing",app.playback->sample());
    else SDL_Log("Rotation %s; angle=%.9g", app.paused ? "paused" : "resumed", app.angle);
}

void camera_log(App& app, const char* input) {
    SDL_Log("Camera: %s; yaw=%.6g; pitch=%.6g; zoom=%.6g", input,
        app.view.yaw, app.view.pitch, app.view.zoom);
}

void reset_view(App& app) {
    app.view.reset();
    app.angle = app.clip ? 0.f : .6f;
    if (app.playback) {
        app.playback->rewind();
        SDL_Log("Animation reset; sample=0; paused");
    }
    app.paused = true;
    app.colour = 0;
    camera_log(app, "reset (motion paused)");
}

void touch_input(App& app, const SDL_TouchFingerEvent& event) {
    int width = 0, height = 0;
    if (!SDL_GetWindowSize(app.window, &width, &height) || width <= 0 || height <= 0) return;
    const float x = event.x * width, y = event.y * height;
    const double seconds = double(event.timestamp)/1'000'000'000.0;
    if (event.type == SDL_EVENT_FINGER_DOWN) app.touch.down(event.fingerID,x,y,seconds);
    else if (event.type == SDL_EVENT_FINGER_MOTION) {
        const float old_yaw = app.view.yaw, old_pitch = app.view.pitch, old_zoom = app.view.zoom;
        app.touch.move(event.fingerID,x,y,float(std::min(width,height)),app.view);
        if (old_yaw != app.view.yaw || old_pitch != app.view.pitch || old_zoom != app.view.zoom)
            app.paused = true;
    } else if (event.type == SDL_EVENT_FINGER_UP) {
        switch (app.touch.up(event.fingerID,seconds)) {
        case camera::Action::tap: change_colour(app,"touch tap"); break;
        case camera::Action::reset: reset_view(app); break;
        case camera::Action::orbit: camera_log(app,"touch orbit"); break;
        case camera::Action::zoom: camera_log(app,"touch zoom"); break;
        default: break;
        }
    } else app.touch.cancel();
}

void cancel_capture(App& app, const char* reason);

void gamepad_camera(App& app, float seconds) {
    if (!app.gamepad) return;
    const auto x = SDL_GetGamepadAxis(app.gamepad,SDL_GAMEPAD_AXIS_LEFTX);
    const auto y = SDL_GetGamepadAxis(app.gamepad,SDL_GAMEPAD_AXIS_LEFTY);
    const auto z = SDL_GetGamepadAxis(app.gamepad,SDL_GAMEPAD_AXIS_RIGHTY);
    const bool moving = camera::stick(x) || camera::stick(y) || camera::stick(z);
    if (moving) {
        cancel_capture(app,"gamepad movement");
        app.paused = true;
        camera::gamepad(app.view,x,y,z,seconds);
    } else if (app.gamepad_moving) camera_log(app,"gamepad orbit/zoom");
    app.gamepad_moving = moving;
}

std::string report_path(const char* name) {
    const char* directory = SDL_GetAndroidInternalStoragePath();
    if (!directory) throw std::runtime_error("No app storage for benchmark output");
    return std::string(directory) + "/" + name;
}

void cancel_capture(App& app, const char* reason) {
    if (!app.capture.active()) return;
    app.capture.cancel();
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Benchmark aborted: %s; no completed report",reason);
}

void begin_capture(App& app) {
    if (app.clip || app.archive_ball) {
        SDL_Log("Benchmark: startup static scenes only; remove animation.pva and restart for the PNG ball benchmark");
        return;
    }
    if (app.capture.active()) { cancel_capture(app,"B pressed again"); return; }
    try {
        for (const char* file : {"benchmark.json","benchmark-frames.csv"}) {
            if (std::remove(report_path(file).c_str()) && errno != ENOENT)
                throw std::runtime_error("Cannot remove previous benchmark output");
        }
        app.touch.cancel();
        if (!SDL_GetWindowSizeInPixels(app.window,&app.capture_width,&app.capture_height) ||
            app.capture_width <= 0 || app.capture_height <= 0)
            throw std::runtime_error("No benchmark viewport");
        reset_view(app);
        app.paused = false;
        app.capture.begin(SDL_GetTicksNS(),5'000'000'000ULL,60'000'000'000ULL);
        SDL_Log("Benchmark started: 5s warmup + 60s capture; viewport=%dx%d; vsync=%d",
            app.capture_width,app.capture_height,app.vsync);
    } catch (const std::exception& error) {
        app.capture.cancel();
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Benchmark start: %s",error.what());
    }
}

void save_capture(App& app) {
    try {
        const auto summary = diagnostics::summarise(app.capture.frames());
        const auto csv_path = report_path("benchmark-frames.csv");
        const auto json_path = report_path("benchmark.json");
        {
            std::ofstream csv(csv_path + ".tmp");
            csv << "end_ns,interval_ns,submit_ns,swap_ns\n";
            for (const auto& f : app.capture.frames())
                csv << f.end_ns << ',' << f.interval_ns << ',' << f.submit_ns << ',' << f.swap_ns << '\n';
            csv.flush();
            if (!csv) throw std::runtime_error("Cannot write frame samples");
        }
        {
            std::ofstream json(json_path + ".tmp");
            json << std::setprecision(12);
            const auto distribution = [&](const char* name,const diagnostics::Distribution& d) {
                json << "  \"" << name << "\": {\"mean_ms\":" << d.mean_ms << ",\"p50_ms\":" << d.p50_ms
                     << ",\"p95_ms\":" << d.p95_ms << ",\"p99_ms\":" << d.p99_ms << ",\"max_ms\":" << d.max_ms << "},\n";
            };
            json << "{\n  \"schema\":1,\n  \"status\":\"complete\",\n  \"scene\":\"" << (app.mesh_loaded ? "mesh" : "cube")
                 << "\",\n  \"width\":" << app.capture_width << ",\"height\":" << app.capture_height
                 << ",\"vsync\":" << (app.vsync ? "true" : "false") << ",\n"
                 << "  \"vertices\":" << app.mesh_vertices << ",\"triangles\":" << app.mesh_triangles
                 << ",\"vertex_crc32\":" << app.mesh_vertex_crc << ",\"index_crc32\":" << app.mesh_index_crc
                 << ",\"texture_crc32\":" << app.texture_crc << ",\n"
                 << "  \"warmup_seconds\":5,\"requested_seconds\":60,\"frames\":" << summary.frames
                 << ",\"seconds\":" << summary.seconds << ",\"fps\":" << summary.fps << ",\n"
                 << "  \"over_16_67_ms\":" << summary.over_16_67_ms << ",\"over_33_33_ms\":" << summary.over_33_33_ms
                 << ",\"over_50_ms\":" << summary.over_50_ms << ",\n"
                 << "  \"startup\": {\"mesh_read_ms\":" << app.mesh_read_ms << ",\"mesh_upload_ms\":" << app.mesh_upload_ms
                 << ",\"image_read_ms\":" << app.image_read_ms << ",\"image_upload_ms\":" << app.image_upload_ms
                 << ",\"create_ms\":" << app.create_ms << ",\"main_to_first_swap_ms\":" << app.first_frame_ms << "},\n";
            distribution("interval",summary.interval);
            distribution("submit",summary.submit);
            distribution("swap",summary.swap);
            json << "  \"percentile_method\":\"nearest_rank\"\n}\n";
            json.flush();
            if (!json) throw std::runtime_error("Cannot write frame summary");
        }
        if (std::rename((csv_path + ".tmp").c_str(),csv_path.c_str()) ||
            std::rename((json_path + ".tmp").c_str(),json_path.c_str()))
            throw std::runtime_error("Cannot publish benchmark files");
        SDL_Log("Benchmark complete: %zu frames; %.6fs; %.3f fps; interval p95=%.3f p99=%.3f max=%.3f ms; >33.33ms=%zu >50ms=%zu",
            summary.frames,summary.seconds,summary.fps,summary.interval.p95_ms,summary.interval.p99_ms,
            summary.interval.max_ms,summary.over_33_33_ms,summary.over_50_ms);
    } catch (const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Benchmark report: %s",error.what());
    }
}

bool load_animation(App& app) {
    const char* directory=SDL_GetAndroidInternalStoragePath();
    if (!directory) return false;
    bool uploaded=false;
    try {
        const auto start=SDL_GetTicksNS();
        auto clip=std::make_unique<animation::Clip>(animation::load_clip(std::string(directory)+"/animation.pva"));
        const auto mesh=clip->mesh(0);
        auto playback=std::make_unique<animation::Playback>(clip->samples.size(),clip->fps);
        const auto upload=SDL_GetTicksNS();
        std::string error;
        if (!app.cube.set_mesh(mesh,error,true)) throw std::runtime_error(error);
        uploaded=true;
        const assets::Image white{1,1,{255,255,255,255}};
        if (!app.cube.set_texture(white,error)) throw std::runtime_error(error);
        app.mesh_read_ms=double(upload-start)/1e6;
        app.mesh_upload_ms=double(SDL_GetTicksNS()-upload)/1e6;
        app.mesh_loaded=true;
        app.mesh_vertices=mesh.vertices.size();app.mesh_triangles=mesh.indices.size()/3;
        app.angle=0;
        SDL_Log("Animation loaded: %zu bones, %zu vertices, %zu triangles, %zu samples at %u fps; final sample held",
            clip->samples[0].size(),mesh.vertices.size(),mesh.indices.size()/3,clip->samples.size(),clip->fps);
        app.clip=std::move(clip);
        app.playback=std::move(playback);
        return true;
    } catch (const std::system_error& e) {
        if (e.code().value()==ENOENT) SDL_Log("Animation: no animation.pva; using static scene");
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Animation: %s; using static scene",e.what());
    } catch (const std::exception& e) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Animation: %s; using static scene",e.what());
    }
    if (uploaded) {
        // Restore the original fallback scene after a partial GL upload.
        app.cube.destroy();
        std::string error;
        if (!app.cube.create(error)) {
            SDL_SetError("%s",error.c_str());
            app.running=false;
        }
    }
    return false;
}

bool load_skinned_model(App& app) {
    const char* directory = SDL_GetAndroidInternalStoragePath();
    if (!directory) return false;
    const std::string path = std::string(directory) + "/player.ktmdl";
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (errno != ENOENT) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Player model: cannot open %s", path.c_str());
        return false;
    }
    try {
        const auto read_begin = SDL_GetTicksNS();
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
        const auto model = assets::decode_skinned_ktmdl(bytes);
        const auto upload_begin = SDL_GetTicksNS();
        std::string error;
        if (!app.cube.set_skinned_model(model, error)) throw std::runtime_error(error);
        bool has_alternate_model = false;
        {
            std::ifstream alternate_input(std::string(directory) + "/player-alt.ktmdl", std::ios::binary);
            if (alternate_input) {
                std::vector<std::uint8_t> alternate_bytes((std::istreambuf_iterator<char>(alternate_input)), {});
                const auto alternate = assets::decode_skinned_ktmdl(alternate_bytes);
                if (!app.cube.add_skinned_model(alternate, error)) throw std::runtime_error(error);
                has_alternate_model = true;
            }
        }
        assets::Image image{1, 1, {255, 255, 255, 255}};
        try {
            image = assets::load_png(std::string(directory) + "/player.png");
            SDL_Log("Player texture loaded: %ux%u RGBA8", image.width, image.height);
        } catch (const std::system_error& texture_error) {
            if (texture_error.code().value() != ENOENT) throw;
            SDL_Log("Player texture: no player.png; using white fallback");
        }
        if (!app.cube.set_texture(image, error)) throw std::runtime_error(error);
        const auto read_pose = [&model](const std::string& path, bool required,
                                        std::uint32_t& trace_call) {
            std::ifstream pose_input(path, std::ios::binary);
            if (!pose_input) {
                if (required) throw std::runtime_error("Cannot open required player pose: " + path);
                return std::vector<skinning::Matrix>{};
            }
            std::vector<std::uint8_t> pose((std::istreambuf_iterator<char>(pose_input)), {});
            if (pose.size() != 16 + model.bones.size() * 16 * sizeof(float) ||
                std::memcmp(pose.data(), "PESPOSE1", 8))
                throw std::runtime_error("Player pose header/size does not match player.ktmdl");
            const auto read_u32 = [&pose](std::size_t offset) {
                return std::uint32_t(pose[offset]) | (std::uint32_t(pose[offset+1]) << 8) |
                    (std::uint32_t(pose[offset+2]) << 16) | (std::uint32_t(pose[offset+3]) << 24);
            };
            if (read_u32(8) != model.bones.size())
                throw std::runtime_error("Player pose skeleton count differs from player.ktmdl");
            trace_call = read_u32(12);
            skinning::Instance first;
            first.bones.resize(model.bones.size());
            std::memcpy(first.bones.data(), pose.data() + 16, first.bones.size() * sizeof(skinning::Matrix));
            for (const auto& matrix : first.bones)
                for (const auto value : matrix)
                    if (!std::isfinite(value)) throw std::runtime_error("Player pose has non-finite matrix data");
            return first.bones;
        };
        const auto field_path = std::string(directory) + "/player-field.pes";
        std::ifstream field_input(field_path, std::ios::binary);
        if (field_input) {
            std::vector<std::uint8_t> field((std::istreambuf_iterator<char>(field_input)), {});
            const auto read_u32 = [&field](std::size_t offset) {
                if (offset + 4 > field.size()) throw std::runtime_error("Truncated player-field package");
                return std::uint32_t(field[offset]) | (std::uint32_t(field[offset+1]) << 8) |
                    (std::uint32_t(field[offset+2]) << 16) | (std::uint32_t(field[offset+3]) << 24);
            };
            if (field.size() < 12 || std::memcmp(field.data(), "PESFLD01", 8) || !has_alternate_model)
                throw std::runtime_error("Player-field header or alternate model is missing");
            const auto count = read_u32(8);
            constexpr std::size_t record_size = 16 + 3 * sizeof(float) + 19 * 16 * sizeof(float);
            if (count == 0 || count > 22 || field.size() != 12 + count * record_size)
                throw std::runtime_error("Player-field actor count or size is invalid");
            app.skinned_instances.clear();
            app.skinned_instances.reserve(count);
            std::size_t cursor = 12;
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto model_index = read_u32(cursor);
                const auto trace_call = read_u32(cursor + 4);
                const auto bone_count = read_u32(cursor + 8);
                if (model_index > 1 || bone_count != 19)
                    throw std::runtime_error("Player-field model index or skeleton size is invalid");
                const std::size_t position_offset = cursor + 16;
                skinning::Instance instance;
                instance.model_index = model_index;
                instance.bones.resize(bone_count);
                const auto* position = reinterpret_cast<const float*>(field.data() + position_offset);
                for (int axis = 0; axis < 3; ++axis) {
                    if (!std::isfinite(position[axis])) throw std::runtime_error("Player-field position is not finite");
                    instance.transform[12 + axis] = position[axis];
                }
                constexpr float instance_scale = 0.08f;
                instance.transform[0] = instance.transform[5] = instance.transform[10] = instance_scale;
                const std::size_t matrices_offset = position_offset + 3 * sizeof(float);
                std::memcpy(instance.bones.data(), field.data() + matrices_offset,
                            instance.bones.size() * sizeof(skinning::Matrix));
                for (const auto& matrix : instance.bones)
                    for (const auto value : matrix)
                        if (!std::isfinite(value)) throw std::runtime_error("Player-field pose is not finite");
                app.skinned_instances.push_back(std::move(instance));
                SDL_Log("Match draw loaded: actor=%u model=%u trace-call=%u", i, model_index, trace_call);
                cursor += record_size;
            }
            SDL_Log("Captured match field loaded: %u instances across two model variants", count);
        }
        std::uint32_t first_call = 0, second_call = 0;
        const auto first_pose = app.skinned_instances.empty()
            ? read_pose(std::string(directory) + "/player.pose", false, first_call)
            : std::vector<skinning::Matrix>{};
        if (!first_pose.empty()) {
            auto second_pose = read_pose(std::string(directory) + "/player2.pose", false, second_call);
            if (second_pose.empty()) {
                second_pose = first_pose;
                second_call = first_call;
            }
            skinning::Instance first, second;
            first.bones = first_pose;
            second.bones = std::move(second_pose);
            first.transform[12] = -1.05f;
            second.transform[12] = 1.05f;
            app.skinned_instances = {std::move(first), std::move(second)};
            SDL_Log("Captured player poses loaded: trace calls=%u,%u; skeleton bones=%zu; drawing two instances",
                    first_call, second_call, model.bones.size());
        }
        app.mesh_read_ms = double(upload_begin - read_begin) / 1e6;
        app.mesh_upload_ms = double(SDL_GetTicksNS() - upload_begin) / 1e6;
        app.mesh_loaded = true;
        app.mesh_vertices = 0;
        app.mesh_triangles = 0;
        for (std::size_t i = 0; i < model.packets.size(); ++i) {
            const auto& packet = model.packets[i];
            app.mesh_vertices += packet.vertex_count;
            app.mesh_triangles += packet.indices.size() / 3;
            for (const auto& texture : packet.textures)
                SDL_Log("Player texture reference: packet=%zu; id=%016llx:%016llx; binding=%u; mode=%u; UV=%u",
                    i, static_cast<unsigned long long>(texture.id_hi),
                    static_cast<unsigned long long>(texture.id_lo), texture.binding_index,
                    texture.mode, texture.uv_channel);
        }
        app.angle = 0;
        SDL_Log("Skinned player loaded: %zu bones; %zu packet vertices; %zu triangles; %s",
            model.bones.size(), app.mesh_vertices, app.mesh_triangles,
            app.skinned_instances.empty() ? "bind pose" : "captured PES draw pose");
        return true;
    } catch (const std::exception& error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Player model: %s; trying other scene", error.what());
        return false;
    }
}

void load_mesh(App& app) {
    const char* directory = SDL_GetAndroidInternalStoragePath();
    if (!directory) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mesh: %s; using cube", SDL_GetError());
        return;
    }
    try {
        const auto read_begin = SDL_GetTicksNS();
        const auto mesh = assets::load_obj(std::string(directory) + "/model.obj");
        const auto upload_begin = SDL_GetTicksNS();
        app.mesh_read_ms = double(upload_begin-read_begin)/1e6;
        std::string error;
        if (!app.cube.set_mesh(mesh, error)) throw std::runtime_error(error);
        app.mesh_upload_ms = double(SDL_GetTicksNS()-upload_begin)/1e6;
        app.mesh_loaded = true;
        app.mesh_vertices = mesh.vertices.size(); app.mesh_triangles = mesh.indices.size()/3;
        app.mesh_vertex_crc = crc32(0,reinterpret_cast<const Bytef*>(mesh.vertices.data()),
            static_cast<uInt>(mesh.vertices.size()*sizeof(assets::MeshVertex)));
        app.mesh_index_crc = crc32(0,reinterpret_cast<const Bytef*>(mesh.indices.data()),
            static_cast<uInt>(mesh.indices.size()*sizeof(std::uint16_t)));
        SDL_Log("Mesh loaded: %zu vertices, %zu triangles; centre=(%.9g,%.9g,%.9g); radius=%.9g",
            mesh.vertices.size(), mesh.indices.size()/3, mesh.centre[0],mesh.centre[1],mesh.centre[2],mesh.radius);
        SDL_Log("Mesh CRC32: vertices=%08lx; indices=%08lx", app.mesh_vertex_crc,app.mesh_index_crc);
    } catch (const std::system_error& error) {
        if (error.code().value() == ENOENT) SDL_Log("Mesh: no model.obj; using cube");
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mesh: %s; using cube", error.what());
    } catch (const std::exception& error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mesh: %s; using cube", error.what());
    }
}

void load_texture(App& app) {
    // The Android adapter chooses the path; the reader has no SDL dependency.
    const char* directory = SDL_GetAndroidInternalStoragePath();
    if (!directory) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Texture: %s; using checkerboard", SDL_GetError());
        return;
    }
    try {
        const char* file = app.mesh_loaded ? "model.png" : "texture.png";
        const auto read_begin = SDL_GetTicksNS();
        const auto image = assets::load_png(std::string(directory) + "/" + file);
        const auto upload_begin = SDL_GetTicksNS();
        app.image_read_ms = double(upload_begin-read_begin)/1e6;
        std::string error;
        if (!app.cube.set_texture(image, error)) throw std::runtime_error(error);
        app.image_upload_ms = double(SDL_GetTicksNS()-upload_begin)/1e6;
        app.texture_crc = crc32(0,image.rgba.data(),static_cast<uInt>(image.rgba.size()));
        SDL_Log("Texture loaded: %ux%u RGBA8; pixels CRC32=%08lx", image.width, image.height,
                app.texture_crc);
    } catch (const std::system_error& error) {
        if (error.code().value() == ENOENT)
            SDL_Log("Texture: no %s; using checkerboard", app.mesh_loaded ? "model.png" : "texture.png");
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Texture: %s; using checkerboard", error.what());
    } catch (const std::exception& error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Texture: %s; using checkerboard", error.what());
    }
}

void show_archive_ball(App& app) {
    Cube replacement;
    try {
        const char* directory = SDL_GetAndroidInternalStoragePath();
        if (!directory) throw std::runtime_error("No app storage for archive ball");
        const std::string root = directory;
        const auto start = SDL_GetTicksNS();
        const auto resource = assets::load_resource(root + "/resource-img", 0x0b000011);
        // Original ball consumer: entry 0 is KTMDL; model ID 2 binds entry 1.
        // This assignment is specific to resource 0x0b000011, not ID minus one.
        const auto entry = assets::bundle_entry(resource.bytes, 0);
        const auto model = assets::decode_static_ktmdl({resource.bytes.begin()+entry.offset,
            resource.bytes.begin()+entry.offset+entry.size});
        const auto& mesh = model.mesh;
        const assets::TextureReference* colour = nullptr;
        for (const auto& ref : model.textures) {
            if (ref.id_hi == 0 && ref.id_lo == 2) {
                if (colour || ref.mode != 0 || ref.uv_channel != 0 || ref.parameter != 0)
                    throw std::runtime_error("Unsupported ball colour binding");
                colour = &ref;
            }
        }
        if (!colour) throw std::runtime_error("Missing ball colour texture ID 2");
        const auto texture_start = SDL_GetTicksNS();
        const auto image = assets::decode_bundle_texture(resource.bytes, 1);
        const auto upload = SDL_GetTicksNS();
        std::string error;
        if (!replacement.create(error) || !replacement.set_mesh(mesh, error) ||
            !replacement.set_texture(image, error)) throw std::runtime_error(error);
        const auto finished = SDL_GetTicksNS();
        // Commit both uploads together so failures preserve the previous scene.
        std::swap(app.cube, replacement);
        app.playback.reset();
        app.clip.reset();
        app.archive_ball = true;
        app.mesh_loaded = true;
        app.mesh_vertices = mesh.vertices.size(); app.mesh_triangles = mesh.indices.size()/3;
        app.mesh_vertex_crc = crc32(0, reinterpret_cast<const Bytef*>(mesh.vertices.data()),
            static_cast<uInt>(mesh.vertices.size()*sizeof(assets::MeshVertex)));
        app.mesh_index_crc = crc32(0, reinterpret_cast<const Bytef*>(mesh.indices.data()),
            static_cast<uInt>(mesh.indices.size()*sizeof(std::uint16_t)));
        app.texture_crc = crc32(0, image.rgba.data(), static_cast<uInt>(image.rgba.size()));
        app.mesh_read_ms = double(texture_start-start)/1e6;
        app.image_read_ms = double(upload-texture_start)/1e6;
        reset_view(app);
        SDL_Log("Archive ball ready: id=0x0b000011; model entry=0; colour ID=2; texture entry=1; %ux%u RGBA8; pixels CRC32=%08lx; vertices=%zu triangles=%zu; upload=%.3f ms; paused",
            image.width, image.height, app.texture_crc, app.mesh_vertices, app.mesh_triangles,
            double(finished-upload)/1e6);
        SDL_Log("Archive ball mesh CRC32: vertices=%08lx; indices=%08lx; model texture references=%zu",
            app.mesh_vertex_crc, app.mesh_index_crc, model.textures.size());
    } catch (const std::exception& error) {
        // Upload failure must not leave a GL error that aborts the retained scene.
        while (glGetError() != GL_NO_ERROR) {}
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Archive ball: %s; previous scene retained", error.what());
    }
    replacement.destroy();
}

void save_new_file(const std::string& path, const void* bytes, std::size_t size) {
    auto* output = std::fopen(path.c_str(), "wbx");
    if (!output) throw std::system_error(errno, std::generic_category(), "Create resource check output");
    const auto written = std::fwrite(bytes, 1, size, output);
    const int closed = std::fclose(output);
    if (written != size || closed) throw std::runtime_error("Resource check output write failed");
}

void check_resources() {
    try {
        const char* directory = SDL_GetAndroidInternalStoragePath();
        if (!directory) throw std::runtime_error("No app storage for resource check");
        const std::string root = directory;
        const auto output_directory = root + "/resource-check";
        // A fresh directory prevents stale results or overwriting earlier checks.
        if (mkdir(output_directory.c_str(), 0700))
            throw std::system_error(errno, std::generic_category(), "Create fresh resource-check directory");
        struct Sample { std::uint32_t id; const char* archive; const char* output; };
        constexpr Sample samples[] = {{0x0b000011, "dt0b.img", "ball.bin"},
                                      {0x0e000001, "dt0e.img", "animation.bin"}};
        std::ostringstream report;
        report << "resource_id,archive,status,entry_count,entry_offset,entry_size,decoded_size,decoded_crc32,output\n";
        unsigned loaded = 0;
        for (const auto& sample : samples) {
            report << "0x" << std::hex << std::setfill('0') << std::setw(8) << sample.id
                   << std::dec << ',' << sample.archive << ',';
            try {
                const auto resource = assets::load_resource(root + "/resource-img", sample.id);
                save_new_file(output_directory + "/" + sample.output, resource.bytes.data(), resource.bytes.size());
                const auto checksum = crc32(0, resource.bytes.data(), static_cast<uInt>(resource.bytes.size()));
                report << "ok," << resource.entry_count << ',' << resource.entry_offset << ','
                       << resource.entry_size << ',' << resource.bytes.size() << ',' << std::hex
                       << std::setw(8) << checksum << std::dec << ',' << sample.output << '\n';
                ++loaded;
                SDL_Log("Resource loaded: id=0x%08x; entry offset=%u size=%u; decoded=%zu; CRC32=%08lx",
                        sample.id, resource.entry_offset, resource.entry_size, resource.bytes.size(), checksum);
            } catch (const std::exception& error) {
                report << "error,,,,,,\n";
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Resource check id=0x%08x: %s", sample.id, error.what());
            }
        }
        const auto text = report.str();
        const auto temporary = output_directory + "/results.csv.tmp";
        save_new_file(temporary, text.data(), text.size());
        if (std::rename(temporary.c_str(), (output_directory + "/results.csv").c_str()))
            throw std::system_error(errno, std::generic_category(), "Publish resource check report");
        SDL_Log("Resource check complete: files/resource-check/results.csv; loaded=%u/2", loaded);
    } catch (const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Resource check failed: %s", error.what());
    }
}

bool create(App& app) {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return false;
    // SDL delivers Android lifecycle notifications directly to event watches.
    if (!SDL_AddEventWatch(lifecycle, &app)) return false;
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) ||
        !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
        !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) return false;
    app.window = SDL_CreateWindow("Pro Evolutica", 960, 540, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!app.window) return false;
    app.context = SDL_GL_CreateContext(app.window);
    if (!app.context) return false;
    app.vsync = SDL_GL_SetSwapInterval(1);
    if (!app.vsync)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vsync: %s", SDL_GetError());
    SDL_Log("GL vendor=%s; renderer=%s; version=%s", glGetString(GL_VENDOR),
            glGetString(GL_RENDERER), glGetString(GL_VERSION));
    std::string error;
    if (!app.cube.create(error)) {
        SDL_SetError("%s", error.c_str());
        return false;
    }
    const char* storage = SDL_GetAndroidInternalStoragePath();
    const bool has_captured_pose = storage &&
        std::ifstream(std::string(storage) + "/player.pose", std::ios::binary).good();
    if (!(has_captured_pose && load_skinned_model(app)) &&
        !load_animation(app) && !load_skinned_model(app)) {
        if (!app.running) return false;
        load_mesh(app);
        load_texture(app);
    }
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads && count > 0) {
        app.gamepad = SDL_OpenGamepad(gamepads[0]);
        if (app.gamepad) SDL_Log("Gamepad: %s", SDL_GetGamepadName(app.gamepad));
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Gamepad: %s", SDL_GetError());
    }
    SDL_free(gamepads);
    SDL_Log("Benchmark: keyboard B starts 5s warmup + 60s capture; input/background/resize cancels");
    SDL_Log("Resource diagnostic: keyboard L reads files/resource-img into new files/resource-check");
    SDL_Log("Archive ball: keyboard T previews dt0b.img resource 0x0b000011 model entry 0 and colour texture entry 1");
    SDL_Log("Controls: drag orbit; pinch zoom; hold/release reset; PS4 left stick orbit, right stick Y zoom, Triangle reset, X tint, Options %s",app.clip ? "play/pause/replay" : "rotation");
    return true;
}

void handle_input(App& app, const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_T) {
        if (!event.key.repeat) {
            cancel_capture(app, "archive ball preview");
            show_archive_ball(app);
        }
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_L) {
        if (!event.key.repeat) {
            cancel_capture(app, "resource check");
            app.paused = true;
            check_resources();
        }
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_B) {
        if (!event.key.repeat) begin_capture(app);
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_FINGER_DOWN ||
        event.type == SDL_EVENT_FINGER_MOTION || event.type == SDL_EVENT_FINGER_UP ||
        event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        cancel_capture(app,"input or viewport change");
    switch (event.type) {
    case SDL_EVENT_QUIT:
        app.running = false;
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        app.touch.cancel();
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_AC_BACK)
            app.running = false;
        else if (event.key.key == SDLK_SPACE && !event.key.repeat) pause_rotation(app);
        else if (!event.key.repeat) {
            const auto key = event.key.key;
            if (key == SDLK_R) reset_view(app);
            else if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN ||
                     key == SDLK_EQUALS || key == SDLK_MINUS) {
                app.paused = true;
                if (key == SDLK_LEFT || key == SDLK_RIGHT) app.view.orbit(key == SDLK_LEFT ? -.15f : .15f,0);
                else if (key == SDLK_UP || key == SDLK_DOWN) app.view.orbit(0,key == SDLK_UP ? -.15f : .15f);
                else app.view.dolly(key == SDLK_EQUALS ? -.15f : .15f);
                camera_log(app,"keyboard");
            }
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        touch_input(app, event.tfinger);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!app.gamepad) {
            app.gamepad = SDL_OpenGamepad(event.gdevice.which);
            if (app.gamepad) SDL_Log("Gamepad: %s", SDL_GetGamepadName(app.gamepad));
            else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Gamepad: %s", SDL_GetError());
        }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (app.gamepad && SDL_GetGamepadID(app.gamepad) == event.gdevice.which) {
            SDL_CloseGamepad(app.gamepad);
            app.gamepad = nullptr;
            app.gamepad_moving = false;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (app.gamepad && event.gbutton.which == SDL_GetGamepadID(app.gamepad)) {
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
                change_colour(app, "gamepad south button");
            else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START) pause_rotation(app);
            else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) reset_view(app);
        }
        break;
    }
}

void destroy(App& app) {
    cancel_capture(app,"app closing");
    SDL_RemoveEventWatch(lifecycle, &app);
    if (app.gamepad) SDL_CloseGamepad(app.gamepad);
    if (app.context) {
        app.cube.destroy();
        SDL_GL_DestroyContext(app.context);
    }
    if (app.window) SDL_DestroyWindow(app.window);
    SDL_Quit();
}
} // namespace

int main(int, char**) {
    App app;
    app.main_begin = SDL_GetTicksNS();
    if (!create(app)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Startup: %s", SDL_GetError());
        destroy(app);
        return 1;
    }
    app.create_ms = double(SDL_GetTicksNS()-app.main_begin)/1e6;
    bool first_frame = true;
    Uint64 previous = SDL_GetTicksNS();
    int result = 0;
    while (app.running) {
        const Uint64 now = SDL_GetTicksNS();
        const bool background_transition=app.clear_touch.exchange(false);
        if (background_transition) {
            app.touch.cancel();
            cancel_capture(app,"background");
        }
        const auto delta=now-previous;
        previous=now;
        // Advance the preceding interval before applying this frame's inputs.
        // Background transitions and the first frame do not consume clip time.
        if (app.playback && !first_frame && app.active && !background_transition && !app.paused) {
            app.playback->advance(delta);
            if (app.playback->ended()) {
                app.paused=true;
                SDL_Log("Animation ended; sample=%zu; held",app.playback->sample());
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) handle_input(app, event);
        if (!app.running) break;
        if (!app.active) {
            previous = SDL_GetTicksNS();
            SDL_Delay(20);
            continue;
        }
        int width = 0, height = 0;
        if (!SDL_GetWindowSizeInPixels(app.window, &width, &height)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window size: %s", SDL_GetError());
            result = 1;
            break;
        }
        if (width <= 0 || height <= 0) {
            SDL_Delay(20);
            continue;
        }
        const float elapsed = std::min(.1f, float(delta) / 1'000'000'000.0f);
        gamepad_camera(app,elapsed);
        if (app.clip && app.uploaded_elapsed_ns!=app.playback->elapsed_ns()) {
            try {
                const auto mesh=app.clip->mesh_at(app.playback->seconds());
                std::string error;
                if (!app.cube.update_vertices(mesh.vertices,error)) throw std::runtime_error(error);
                app.uploaded_sample=app.playback->sample();
                app.uploaded_elapsed_ns=app.playback->elapsed_ns();
            } catch (const std::exception& e) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Animation frame: %s",e.what());
                result=1;
                break;
            }
        }
        if (!app.clip && !app.paused) app.angle = std::fmod(app.angle + elapsed * .65f, 6.28318530718f);
        const bool measuring = app.capture.active();
        const auto submit_begin = measuring ? SDL_GetTicksNS() : 0;
        if (app.skinned_instances.empty())
            app.cube.draw(width, height, app.colour, app.angle, app.view);
        else
            app.cube.draw_skinned_instances(width, height, app.angle, app.view, app.skinned_instances);
        const GLenum error = glGetError();
        const auto swap_begin = measuring ? SDL_GetTicksNS() : 0;
        if (error != GL_NO_ERROR || !SDL_GL_SwapWindow(app.window)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Frame: GL=0x%x; SDL=%s",
                         error, SDL_GetError());
            result = 1;
            break;
        }
        const auto swap_end = (measuring || first_frame) ? SDL_GetTicksNS() : 0;
        if (first_frame) {
            app.first_frame_ms = double(swap_end-app.main_begin)/1e6;
            SDL_Log("Startup ms: mesh read=%.3f upload=%.3f; image read=%.3f upload=%.3f; create=%.3f main-to-first-swap=%.3f",
                app.mesh_read_ms,app.mesh_upload_ms,app.image_read_ms,app.image_upload_ms,app.create_ms,app.first_frame_ms);
            if (app.clip) SDL_Log("Animated mesh frame submitted: %dx%d; sample=%zu",width,height,app.uploaded_sample);
            else SDL_Log("Textured %s frame submitted: %dx%d", app.mesh_loaded ? "mesh" : "cube", width, height);
            first_frame = false;
        }
        if (!app.vsync) SDL_Delay(16);
        if (measuring) {
            try {
                if (app.capture.frame(SDL_GetTicksNS(),swap_begin-submit_begin,swap_end-swap_begin)) save_capture(app);
            } catch (const std::exception& failure) {
                app.capture.cancel();
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Benchmark aborted: %s",failure.what());
            }
        }
    }
    destroy(app);
    return result;
}
