#include <cstdio>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define MODEL_IMPLEMENTATION
#define RENDER3D_IMPLEMENTATION
#include "core.h"

#define WIDTH 800
#define HEIGHT 600
#define MAX_MODELS 16

typedef struct {
    Window_t win;
    Camera cam;
    Input input;
    Renderer renderer;
    Model models[MAX_MODELS];
    int num_models;
    bool running;
    float rotation;
} state_t;

static state_t state = {};

static void update()
{
    if (pollEvents(&state.win, &state.input)) {
        state.running = false;
        return;
    }

    if (isKeyDown(&state.input, KEY_ESCAPE)) { state.running = false; return; }

    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.display, state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.display, state.win.window, state.win.width, state.win.height, &state.input);

    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    if (isMouseGrabbed(&state.input)) cameraRotate(&state.cam, dx * 0.3f, -dy * 0.3f);

    const float speed = isKeyDown(&state.input, KEY_LSHIFT) ? 0.2f : 0.05f;
    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, speed);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front, -1.0f), speed);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right, -1.0f), speed);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, speed);

    // Rotate model
    state.rotation += 0.01f;
    for (int i = 0; i < state.num_models; i++) {
        modelTransform(&state.models[i],
            state.models[i].position,
            vec3(0.0f, state.rotation, 0.0f),
            state.models[i].scale);
    }
    modelUpdate(state.models, state.num_models);
}

static void render()
{
    renderClear(&state.renderer);
    renderScene(&state.renderer, state.models, state.num_models);
    updateFramebuffer(&state.win);
}

int main()
{
    printf("X11 Model Example - Software 3D Rasterization\n");
    printf("==============================================\n\n");

    windowInit(&state.win);
    state.win.width = WIDTH;
    state.win.height = HEIGHT;
    state.win.title = "X11 Model - cube.obj";

    if (!createWindow(&state.win)) {
        fprintf(stderr, "Failed to create X11 window\n");
        return 1;
    }
    printf("X11 window created: %dx%d\n", WIDTH, HEIGHT);

    cameraInit(&state.cam);
    state.cam.position = vec3(1.0f, 1.4f, -4.7f);
    state.cam.yaw = 102.0f;
    state.cam.pitch = -17.0f;
    state.cam.fov = 60.0f;
    cameraUpdate(&state.cam);
    printf("Camera initialized at (0, 0, -5)\n");

    inputInit(&state.input);
    printf("Input initialized\n");

    renderInit(&state.renderer, &state.win, &state.cam);
    state.renderer.backface_culling = true;
    state.renderer.light = true;
    state.renderer.light_dir = norm(vec3(0.5f, -1.0f, 0.5f));
    state.renderer.wireframe = false;
    printf("Renderer initialized (light=%d, culling=%d)\n",
        state.renderer.light, state.renderer.backface_culling);

    printf("\nLoading cube.obj...\n");
    Model* cube = modelCreate(state.models, &state.num_models, MAX_MODELS,
        vec3(0.8f, 0.4f, 0.2f), 0.0f, 0.5f);
    if (cube) {
        modelLoad(cube, "examples/x11_model/cube.obj");
        modelTransform(cube, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), vec3(2.0f, 2.0f, 2.0f));
        modelUpdate(state.models, state.num_models);
        printf("Model loaded: %d triangles\n", cube->num_triangles);
    } else {
        fprintf(stderr, "Failed to create model (max models reached?)\n");
    }

    state.running = true;
    state.rotation = 0.0f;

    printf("\nStarting main loop...\n");
    printf("Press ESC or close window to exit\n\n");

    while (state.running) {
        update();
        render();
        updateFrame(&state.win);
    }

    printf("\nCleaning up...\n");
    for (int i = 0; i < state.num_models; i++) modelFree(&state.models[i]);
    renderFree(&state.renderer);
    destroyWindow(&state.win);
    printf("Done!\n");

    return 0;
}