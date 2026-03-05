#include <cstdio>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#include "core.h"

#define WIDTH 1270
#define HEIGHT 850

typedef struct {
    float positions[3][4];
    float colors[3][4];
} TriangleVertexUniforms;

typedef struct {
    float global_tint[4];
} TriangleFragmentUniforms;

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

    TriangleVertexUniforms v = {
        .positions = {
            { 0.0f,  0.72f, 0.0f, 0.0f},
            {-0.72f, -0.72f, 0.0f, 0.0f},
            { 0.72f, -0.72f, 0.0f, 0.0f},
        },
        .colors = {
            {1.00f, 0.30f, 0.20f, 0.0f},
            {0.15f, 0.85f, 0.35f, 0.0f},
            {0.15f, 0.40f, 1.00f, 0.0f},
        },
    };

    TriangleFragmentUniforms f = {
        .global_tint = {1.0f, 1.0f, 1.0f, 0.0f},
    };

    return gpuRenderPipeline(
        &state.gpu,
        state.pipeline,
        &v,
        (Uint32)sizeof(v),
        &f,
        (Uint32)sizeof(f),
        3);
}

int main()
{
    windowInit(&state.win);
    state.win.width = WIDTH;
    state.win.height = HEIGHT;
    state.win.title = "triangle test";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(0.0f, 3.0f, 10.0f);
    state.cam.yaw = -90.0f;
    state.cam.pitch = -20.0f;
    state.cam.fov = 75.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    ASSERT(gpuInit(&state.gpu, &state.win));
    state.pipeline = gpuCreatePipeline(&state.gpu, "basic_triangle", 1, 1);
    ASSERT(state.pipeline);
    state.running = true;
    state.move_speed = 0.1f;
    state.mouse_sensitivity = 0.3f;

    while (state.running) {
        update();

        state.ticks = (float)SDL_GetTicks() * 0.001f;
        if (!render()) {
            state.running = false;
        }

        updateFrame(&state.win);
    }

    cleanup();
    return 0;
}
