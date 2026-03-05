#include <cstdio>
#include <cstring>
#include <cmath>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#include "core.h"

#define WIDTH 1270
#define HEIGHT 850

typedef struct {
    float cam_pos[4];
    float cam_right[4];
    float cam_up[4];
    float cam_forward[4];
    float screen[4]; // x=width, y=height, z=time_sec, w=tan_half_fov
    float render_cfg[4];  // x=aspect, y=grid_size, z=max_dist, w=max_steps
} VoxelUniforms;

typedef struct {
    Window_t win;
    Camera cam;
    Input input;
    Gpu gpu;
    SDL_GPUGraphicsPipeline *pipeline;

    bool running;
    float ticks;
    float move_speed;
    float mouse_sensitivity;
} state_t;

static state_t state = {0};

#define cleanup() do { \
    gpuReleasePipeline(&state.gpu, &state.pipeline); \
    gpuFree(&state.gpu); \
    destroyWindow(&state.win); \
} while(0)

static void update()
{
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    cameraRotate(&state.cam, (float)dx * state.mouse_sensitivity, (float)(-dy) * state.mouse_sensitivity);

    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, state.move_speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front, -1.0f), state.move_speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right, -1.0f), state.move_speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, state.move_speed);
}

static bool render()
{
    if (!state.pipeline) return false;

    int width = 0, height = 0;
    if (!gpuGetDrawableSize(&state.gpu, &width, &height)) return false;

    const float aspect = (height > 0) ? ((float)width / (float)height) : 1.0f;
    const float tan_half_fov = tanf((state.cam.fov * PI / 180.0f) * 0.5f);

    const auto u = (VoxelUniforms) {
        .cam_pos = {state.cam.position.x, state.cam.position.y, state.cam.position.z},
        .cam_right = {state.cam.right.x, state.cam.right.y, state.cam.right.z},
        .cam_up = {state.cam.up.x, state.cam.up.y, state.cam.up.z},
        .cam_forward = {state.cam.front.x, state.cam.front.y, state.cam.front.z},
        .screen = {(float)width, (float)height, state.ticks, tan_half_fov},
        .render_cfg = {aspect, 1.0f, 140.0f, 256.0f},
    };

    return gpuRenderPipeline(&state.gpu, state.pipeline, NULL, 0, &u, (Uint32)sizeof(u), 3);
}

int main()
{
    windowInit(&state.win);
    state.win.width = WIDTH;
    state.win.height = HEIGHT;
    state.win.title = "raycast";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(10.0f, 14.0f, 24.0f);
    state.cam.yaw = -114.0f;
    state.cam.pitch = -40.0f;
    state.cam.fov = 75.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    ASSERT(gpuInit(&state.gpu, &state.win));

    state.pipeline = gpuCreatePipeline(&state.gpu, "voxel_raymarch", 0, 1);
    ASSERT(state.pipeline);

    state.running = true;
    state.move_speed = 0.1f;
    state.mouse_sensitivity = 0.3f;

    while (state.running) {
        update();
        state.ticks = (float)SDL_GetTicks() * 0.001f;
        if (!render()) state.running = false;
        updateFrame(&state.win);
    }

    cleanup();
    return 0;
}
