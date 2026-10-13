#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../rendering/triangle.h"
#include <atomic>

namespace {
struct App {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_Gamepad* gamepad = nullptr;
    Triangle triangle;
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

bool create(App& app) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return false;
    SDL_SetEventFilter(lifecycle, &app);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
        !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) ||
        !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) return false;
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
    if (!app.triangle.create(error)) {
        SDL_SetError("%s", error.c_str());
        return false;
    }
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
        app.triangle.destroy();
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
    int result = 0;
    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handle_input(app, event);
        if (!app.running) break;
        if (!app.active) {
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
        app.triangle.draw(width, height, app.colour);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR || !SDL_GL_SwapWindow(app.window)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Frame: GL=0x%x; SDL=%s",
                         error, SDL_GetError());
            result = 1;
            break;
        }
        if (first_frame) {
            SDL_Log("Triangle frame submitted: %dx%d", width, height);
            first_frame = false;
        }
        if (!app.vsync) SDL_Delay(16);
    }
    destroy(app);
    return result;
}
