#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../rendering/cube.h"
#include "../diagnostics/frame_stats.h"
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

namespace {
struct App {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_Gamepad* gamepad = nullptr;
    Cube cube;
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
    app.paused = !app.paused;
    SDL_Log("Rotation %s; angle=%.9g", app.paused ? "paused" : "resumed", app.angle);
}

void camera_log(App& app, const char* input) {
    SDL_Log("Camera: %s; yaw=%.6g; pitch=%.6g; zoom=%.6g", input,
        app.view.yaw, app.view.pitch, app.view.zoom);
}

void reset_view(App& app) {
    app.view.reset();
    app.angle = .6f;
    app.paused = true;
    app.colour = 0;
    camera_log(app, "reset (rotation paused)");
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
    load_mesh(app);
    load_texture(app);
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads && count > 0) {
        app.gamepad = SDL_OpenGamepad(gamepads[0]);
        if (app.gamepad) SDL_Log("Gamepad: %s", SDL_GetGamepadName(app.gamepad));
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Gamepad: %s", SDL_GetError());
    }
    SDL_free(gamepads);
    SDL_Log("Benchmark: keyboard B starts 5s warmup + 60s capture; input/background/resize cancels");
    SDL_Log("Controls: drag orbit; pinch zoom; hold/release reset; PS4 left stick orbit, right stick Y zoom, Triangle reset, X tint, Options rotation");
    return true;
}

void handle_input(App& app, const SDL_Event& event) {
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
        if (app.clear_touch.exchange(false)) {
            app.touch.cancel();
            cancel_capture(app,"background");
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
        const Uint64 now = SDL_GetTicksNS();
        const float elapsed = std::min(.1f, float(now - previous) / 1'000'000'000.0f);
        previous = now;
        gamepad_camera(app,elapsed);
        if (!app.paused) app.angle = std::fmod(app.angle + elapsed * .65f, 6.28318530718f);
        const bool measuring = app.capture.active();
        const auto submit_begin = measuring ? SDL_GetTicksNS() : 0;
        app.cube.draw(width, height, app.colour, app.angle, app.view);
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
            SDL_Log("Textured %s frame submitted: %dx%d", app.mesh_loaded ? "mesh" : "cube", width, height);
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
