#include <SDL3/SDL.h>
#include <imgui.h>
#include <iostream>
#include <vector>
#include <thread>
#include <future>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define MODEL_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#include "core.h"

#define MAX(a, b) (( (a) > (b) ) ? (a) : (b))
#define MIN(a, b) (( (a) < (b) ) ? (a) : (b))

#define WIDTH 800
#define HEIGHT 600
#define RENDER_SCALE 0.5f

// Prepared triangle
typedef struct {
    Vec3 v0, e1, e2;
    Vec3 color;
} tri_t;

// state_t
typedef struct
{
    Window_t win;
    SDL_Texture* texture;

    Camera cam;
    Input input;

    std::vector<tri_t> scene_tris;

    bool running;
    float move_speed;
    float mouse_sensitivity;
    int num_threads;
} state_t;

state_t state = {};

#define cleanup() do { \
    if (state.texture) SDL_DestroyTexture(state.texture); \
    destroyWindow(&state.win); \
} while(0)

// Ray-triangle intersection macro
bool ray_triangle_intersect(const Ray &ray, const tri_t* tri, float* t)
{
    const Vec3 h = cross(ray.direction, tri->e2);
    const float a = dot(tri->e1, h);
    if (a > -0.00001f && a < 0.00001f) return false;

    const float f = 1.0f / a;
    const Vec3 s = sub(ray.origin, tri->v0);
    const float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    const Vec3 q = cross(s, tri->e1);
    if (const float v = f * dot(ray.direction, q); v < 0.0f || u + v > 1.0f) return false;

    if (const float _t = f * dot(tri->e2, q); _t > 0.00001f) { *t = _t; return true; }
    return false;
}

// Trace ray macro
Vec3 trace_ray(const Ray& ray)
{
    float min_t = 1e10f;
    const tri_t* hit = nullptr;
    for (const auto& tri : state.scene_tris)
    {
        float t;
        if (ray_triangle_intersect(ray, &tri, &t))
            if (t < min_t) { min_t = t; hit = &tri; }
    }
    if (!hit) return vec3(0,0,0);
    return hit->color;
}

void render_frame()
{
    const float aspect = static_cast<float>(state.win.bWidth) / static_cast<float>(state.win.bHeight);
    const float vp_height = 2.0f * tanf(static_cast<float>(state.cam.fov * M_PI / 180.0f) / 2.0f);
    const float vp_width = vp_height * aspect;

    auto render_rows = [&](const int y0, const int y1)
    {
        for (int y = y0; y < y1; y++)
        {
            for (int x = 0; x < state.win.bWidth; x++)
            {
                const float u = (static_cast<float>(x) / static_cast<float>(state.win.bWidth - 1) - 0.5f) * vp_width;
                const float v = (0.5f - static_cast<float>(y) / static_cast<float>(state.win.bHeight - 1)) * vp_height;

                const Ray ray = cameraGetRay(&state.cam, u, v);
                const auto [cx, cy, cz] = trace_ray(ray);

                const auto r = static_cast<uint8_t>(fminf(cx, 1) * 255);
                const auto g = static_cast<uint8_t>(fminf(cy, 1) * 255);
                const auto b = static_cast<uint8_t>(fminf(cz, 1) * 255);

                state.win.buffer[y * state.win.bWidth + x] = (0xFF<<24)|(r<<16)|(g<<8)|b;
            }
        }
    };

    std::vector<std::future<void>> jobs;
    const int rows = state.win.bHeight / state.num_threads;

    for (int i = 0; i < state.num_threads; i++) {
        int s = i * rows;
        int e = (i == state.num_threads-1) ? state.win.bHeight : (i+1)*rows;
        jobs.push_back(std::async(std::launch::async, render_rows, s, e));
    }
    for (auto& j : jobs) j.wait();
}

void handle_resize()
{
    if (!state.win.resized) return;

    // Update buffer dimensions based on the new window size
    state.win.bWidth  = static_cast<int>(RENDER_SCALE * static_cast<float>(state.win.width));
    state.win.bHeight = static_cast<int>(RENDER_SCALE * static_cast<float>(state.win.height));

    // Recreate framebuffer
    ASSERT(resizeBuffer(&state.win));

    // Recreate texture
    if (state.texture) SDL_DestroyTexture(state.texture);

    state.texture = SDL_CreateTexture(
        state.win.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        state.win.bWidth,
        state.win.bHeight
    );

    ASSERT(state.texture);

    LOG("Resized to " << state.win.width << "x" << state.win.height <<
        " (buffer: " << state.win.bWidth << "x" << state.win.bHeight << ")");

    state.win.resized = false;
}

void update()
{
    // Poll events (this now automatically calls updateInput)
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    // Handle window resize
    handle_resize();

    // Mouse grab control
    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    // Camera rotation
    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    cameraRotate(&state.cam, static_cast<float>(dx) * state.mouse_sensitivity, static_cast<float>(-dy) * state.mouse_sensitivity);

    // Camera movement
    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front,-1), state.move_speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right,-1), state.move_speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

int main() {
    windowInit(&state.win);
    state.win.width   = WIDTH;
    state.win.height  = HEIGHT;
    state.win.bWidth  = static_cast<int>(WIDTH  * RENDER_SCALE);
    state.win.bHeight = static_cast<int>(HEIGHT * RENDER_SCALE);
    state.win.title = "ray";

    ASSERT(createWindow(&state.win));

    state.texture = SDL_CreateTexture(
        state.win.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        state.win.bWidth,
        state.win.bHeight
    );

    ASSERT(state.texture);

    cameraInit(&state.cam);
    state.cam.position = vec3(0,3,10);
    state.cam.yaw = -90;
    state.cam.pitch = -20;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    state.running = true;
    state.move_speed = 0.1f;
    state.mouse_sensitivity = 0.3f;
    state.num_threads = static_cast<int>(std::thread::hardware_concurrency());
    if (!state.num_threads) state.num_threads = 4;

    {   // Single test triangle in front of the camera
        const Vec3 v0 = vec3(-1.0f, 0.0f, 0.0f);
        const Vec3 v1 = vec3( 1.0f, 0.0f, 0.0f);
        const Vec3 v2 = vec3( 0.0f, 1.0f, 0.0f);
        const Vec3 color = vec3(0.2f, 0.8f, 0.4f);
        state.scene_tris.push_back({ v0, sub(v1, v0), sub(v2, v0), color });
    }

    while (state.running)
    {
        update();

        render_frame();
        ASSERT(updateFramebuffer(&state.win, state.texture));

        imguiNewFrame();
            ImGui::Begin("status");
            ImGui::Text("Camera pos: %.2f, %.2f, %.2f", state.cam.position.x, state.cam.position.y, state.cam.position.z);
            ImGui::Text("Fps: %.2f", getFPS(&state.win));
            ImGui::Text("Delta: %.4f ms", getDelta(&state.win) * 1000.0);
            ImGui::Text("Triangles: %zu", state.scene_tris.size());
            ImGui::Text("Resolution: %dx%d (buffer: %dx%d)", state.win.width, state.win.height, state.win.bWidth, state.win.bHeight);
            ImGui::End();
        imguiEndFrame(&state.win);

        SDL_RenderPresent(state.win.renderer);
        updateFrame(&state.win);
    }

    // Cleanup
    cleanup();
    return 0;
}
