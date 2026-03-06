#include <cstdio>

#define CORE_IMPLEMENTATION
#define MATH_IMPLEMENTATION
#define KEYS_IMPLEMENTATION
#define CAMERA_IMPLEMENTATION
#define MODEL_IMPLEMENTATION
#define RENDER3D_IMPLEMENTATION
#define SDL_IMPLEMENTATION
#define IMGUI_IMPLEMENTATION
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
    pollEvents(&state.win, &state.input);

    if (isKeyDown(&state.input, KEY_ESCAPE)) state.running = false;
    if (isKeyDown(&state.input, KEY_LSHIFT)) releaseMouse(state.win.window, &state.input);
    else if (!isMouseGrabbed(&state.input)) grabMouse(state.win.window, state.win.width, state.win.height, &state.input);

    int dx, dy;
    getMouseDelta(&state.input, &dx, &dy);
    if (isMouseGrabbed(&state.input)) cameraRotate(&state.cam, dx * 0.3f, -dy * 0.3f);

    if (isKeyDown(&state.input, KEY_W)) cameraMove(&state.cam, state.cam.front, 0.05f);
    if (isKeyDown(&state.input, KEY_S)) cameraMove(&state.cam, mul(state.cam.front, -1.0f), 0.05f);
    if (isKeyDown(&state.input, KEY_A)) cameraMove(&state.cam, mul(state.cam.right, -1.0f), 0.05f);
    if (isKeyDown(&state.input, KEY_D)) cameraMove(&state.cam, state.cam.right, 0.05f);

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

    static SDL_Texture *texture = NULL;
    if (!texture) {
        texture = SDL_CreateTexture(state.win.renderer,
            SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
            state.win.bWidth, state.win.bHeight);
    }
    updateFramebuffer(&state.win, texture);

    imguiNewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("STATE", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
    ImGui::SeparatorText("Camera");
    ImGui::Text("Pos  %.2f  %.2f  %.2f", state.cam.position.x, state.cam.position.y, state.cam.position.z);
    ImGui::Text("Yaw  %.1f    Pitch  %.1f", state.cam.yaw, state.cam.pitch);
    ImGui::SeparatorText("Info");
    ImGui::Text("FPS: %.1f  |  Time: %.2fs", ImGui::GetIO().Framerate, SDL_GetTicks() * 0.001f);
    ImGui::End();
    imguiEndFrame(&state.win);

    SDL_RenderPresent(state.win.renderer);
}

int main()
{
    windowInit(&state.win);
    state.win.width = WIDTH;
    state.win.height = HEIGHT;
    state.win.title = "SDL CPU Model";

    ASSERT(createWindow(&state.win));

    cameraInit(&state.cam);
    state.cam.position = vec3(1.0f, 1.4f, -4.7f);
    state.cam.yaw = 102.0f;
    state.cam.pitch = -17.0f;
    state.cam.fov = 60.0f;
    cameraUpdate(&state.cam);

    inputInit(&state.input);

    imguiInit(&state.win, state.win.renderer);

    renderInit(&state.renderer, &state.win, &state.cam);
    state.renderer.light_dir = norm(vec3(0.5f, -1.0f, 0.5f));

    Model* cube = modelCreate(state.models, &state.num_models, MAX_MODELS, vec3(0.8f, 0.4f, 0.2f), 0.0f, 0.5f);
    modelLoad(cube, "examples/sdl_cpu_model/cube.obj");
    modelTransform(cube, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), vec3(2.0f, 2.0f, 2.0f));
    modelUpdate(state.models, state.num_models);

    state.running = true;
    state.rotation = 0.0f;

    while (state.running) {
        update();
        render();
        updateFrame(&state.win);
    }

    for (int i = 0; i < state.num_models; i++) modelFree(&state.models[i]);
    renderFree(&state.renderer);
    destroyWindow(&state.win);
    return 0;
}