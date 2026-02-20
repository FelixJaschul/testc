#include <imgui.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
#include "wrapper/core.h"

#define WIDTH 800
#define HEIGHT 600
#define RENDER_SCALE 0.5f

// Prepared triangle
typedef struct {
    Vec3 v0, e1, e2;
    Vec3 color;
} tri_t;

// state_t
typedef struct {
    Window_t win;
    SDL_Texture* texture;

    Camera cam;
    Input input;

    tri_t tri;

    bool running;
    float move_speed;
    float mouse_sensitivity;
} state_t;

state_t state = {0};

#define cleanup() do { \
    if (state.texture) SDL_DestroyTexture(state.texture); \
    destroyWindow(&state.win); \
} while(0)

// Ray-triangle intersection macro
static bool ray_triangle_intersect(const Ray* ray, 
                                   const tri_t* tri, 
                                   float* t)
{
    const Vec3 h = cross(ray->direction, tri->e2);
    const float a = dot(tri->e1, h);
    if (a > -0.00001f && a < 0.00001f) return false;

    const float f = 1.0f / a;
    const Vec3 s = sub(ray->origin, tri->v0);
    const float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    const Vec3 q = cross(s, tri->e1);
    const float v = f * dot(ray->direction, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    if (const float _t = f * dot(tri->e2, q); _t > 0.00001f) { *t = _t; return true; }
    return false;
}

// Trace ray macro
static Vec3 trace_ray(const Ray* ray)
{
    float t;
    if (ray_triangle_intersect(ray, &state.tri, &t))
        return state.tri.color;
    return vec3(0,0,0);
}

static void render_frame()
{
    const float aspect = (float)state.win.bWidth / (float)state.win.bHeight;
    const float vp_height = 2.0f * tanf((float)(state.cam.fov * M_PI / 180.0f) / 2.0f);
    const float vp_width = vp_height * aspect;

    for (int y = 0; y < state.win.bHeight; y++)
    {
        for (int x = 0; x < state.win.bWidth; x++)
        {
            const float u = ((float)x / (float)(state.win.bWidth - 1) - 0.5f) * vp_width;
            const float v = (0.5f - (float)y / (float)(state.win.bHeight - 1)) * vp_height;

            const Ray ray = cameraGetRay(&state.cam, u, v);
            const Vec3 c = trace_ray(&ray);

            const uint8_t r = (uint8_t)(fminf(c.x, 1.0f) * 255.0f);
            const uint8_t g = (uint8_t)(fminf(c.y, 1.0f) * 255.0f);
            const uint8_t b = (uint8_t)(fminf(c.z, 1.0f) * 255.0f);

            state.win.buffer[y * state.win.bWidth + x] = (0xFF<<24)|(r<<16)|(g<<8)|b;
        }
    }
}

static void update()
{
    // Poll events (this now automatically calls updateInput)
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    // Mouse grab control
    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    // Camera rotation
    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    cameraRotate(&state.cam, (float)dx * state.mouse_sensitivity, (float)(-dy) * state.mouse_sensitivity);

    // Camera movement
    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front,-1), state.move_speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right,-1), state.move_speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

int main() 
{
    windowInit(&state.win);
    state.win.width   = WIDTH;
    state.win.height  = HEIGHT;
    state.win.bWidth  = (int)(WIDTH  * RENDER_SCALE);
    state.win.bHeight = (int)(HEIGHT * RENDER_SCALE);
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

    {   // Single test triangle in front of the camera
        const Vec3 v0 = vec3(-1.0f, 0.0f, 0.0f);
        const Vec3 v1 = vec3( 1.0f, 0.0f, 0.0f);
        const Vec3 v2 = vec3( 0.0f, 1.0f, 0.0f);
        const Vec3 color = vec3(0.2f, 0.8f, 0.4f);
        state.tri = { v0, sub(v1, v0), sub(v2, v0), color };
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
