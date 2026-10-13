#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../rendering/cube.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <system_error>
#include <stdexcept>
#include <zlib.h>

namespace {
struct App {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_Gamepad* gamepad = nullptr;
    Cube cube;
    int colour = 0;
    bool vsync = false;
    // Android can deliver lifecycle events from a different thread.
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

void load_texture(App& app) {
    // The Android adapter chooses the path; the reader has no SDL dependency.
    const char* directory = SDL_GetAndroidInternalStoragePath();
    if (!directory) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Texture: %s; using checkerboard", SDL_GetError());
        return;
    }
    try {
        const auto image = assets::load_png(std::string(directory) + "/texture.png");
        std::string error;
        if (!app.cube.set_texture(image, error)) throw std::runtime_error(error);
        SDL_Log("Texture loaded: %ux%u RGBA8; pixels CRC32=%08lx", image.width, image.height,
                crc32(0, image.rgba.data(), static_cast<uInt>(image.rgba.size())));
    } catch (const std::system_error& error) {
        if (error.code().value() == ENOENT)
            SDL_Log("Texture: no texture.png; using checkerboard");
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Texture: %s; using checkerboard", error.what());
    } catch (const std::exception& error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Texture: %s; using checkerboard", error.what());
    }
}

bool create(App& app) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return false;
    SDL_SetEventFilter(lifecycle, &app);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) ||
        !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
        !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) return false;
    app.window = SDL_CreateWindow("Pro Evolutica", 960, 540, SDL_WINDOW_OPENGL);
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
    load_texture(app);
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads && count > 0) {
        app.gamepad = SDL_OpenGamepad(gamepads[0]);
        if (app.gamepad) SDL_Log("Gamepad: %s", SDL_GetGamepadName(app.gamepad));
        else SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Gamepad: %s", SDL_GetError());
    }
    SDL_free(gamepads);
    return true;
}

void handle_input(App& app, const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
        app.running = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_AC_BACK)
            app.running = false;
        break;
    case SDL_EVENT_FINGER_DOWN:
        change_colour(app, "touch");
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
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (app.gamepad && event.gbutton.which == SDL_GetGamepadID(app.gamepad) &&
            event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
            change_colour(app, "gamepad south button");
        break;
    }
}

void destroy(App& app) {
    SDL_SetEventFilter(nullptr, nullptr);
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
    if (!create(app)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Startup: %s", SDL_GetError());
        destroy(app);
        return 1;
    }
    bool first_frame = true;
    Uint64 previous = SDL_GetTicksNS();
    float angle = .6f;
    int result = 0;
    while (app.running) {
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
        angle = std::fmod(angle + elapsed * .65f, 6.28318530718f);
        app.cube.draw(width, height, app.colour, angle);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR || !SDL_GL_SwapWindow(app.window)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Frame: GL=0x%x; SDL=%s",
                         error, SDL_GetError());
            result = 1;
            break;
        }
        if (first_frame) {
            SDL_Log("Textured cube frame submitted: %dx%d", width, height);
            first_frame = false;
        }
        if (!app.vsync) SDL_Delay(16);
    }
    destroy(app);
    return result;
}
